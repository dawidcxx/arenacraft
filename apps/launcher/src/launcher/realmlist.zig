//! Which logon server `Wow.exe` connects to.
//!
//! A 3.3.5a client reads `Data/<locale>/realmlist.wtf`; very old installs keep
//! one at the game root, so both shapes are handled. The file holds one active
//! `set realmlist <host>` line and usually a few commented-out predecessors
//! from earlier manual edits:
//!
//!     set realmlist logon.arenacraft.fun
//!     #set realmlist logon.warmane.com
//!     #set realmlist 62.83.16.59
//!
//! The launcher treats those lines as its realm list. Switching realms rewrites
//! every realmlist file of the installation with the selected host active and
//! keeps every host it has seen as a commented-out line, so the player can go
//! back without retyping an address.
//!
//! Everything here is plain `std` (no Win32), which is why the module has its
//! own native test target in `build.zig`.

const std = @import("std");

/// Realms the launcher knows before it ever looks at the file system. The
/// order is the order of the realm list when the installation has no history.
pub const presets = [_]Preset{
    .{ .name = "Arenacraft", .host = "logon.arenacraft.fun" },
    .{ .name = "Warmane", .host = "logon.warmane.com" },
};

pub const Preset = struct {
    name: []const u8,
    host: []const u8,
};

/// Upper bound for a `realmlist.wtf`; the real file is a few hundred bytes.
const max_file_size: std.Io.Limit = .limited(64 * 1024);

/// Longest accepted realm address. Host names are capped at 253 bytes.
const max_host_len = 253;

/// One realm (logon host) and the file order it should be written in.
pub const List = struct {
    allocator: std.mem.Allocator,
    hosts: std.ArrayListUnmanaged([]u8) = .empty,
    /// Built-in realms the player removed. They are written as `#hidden` lines
    /// (ignored by the client, like any comment) so `ensurePresets` does not
    /// bring them back on the next start.
    hidden: std.ArrayListUnmanaged([]u8) = .empty,
    /// Index into `hosts` of the realm written as the active line.
    active: usize = 0,
    /// Whether any file had an uncommented `set realmlist` line. A list made of
    /// only commented lines still renders its first host as active.
    has_active: bool = false,

    pub fn init(allocator: std.mem.Allocator) List {
        return .{ .allocator = allocator };
    }

    pub fn deinit(self: *List) void {
        for (self.hosts.items) |host| self.allocator.free(host);
        self.hosts.deinit(self.allocator);
        for (self.hidden.items) |host| self.allocator.free(host);
        self.hidden.deinit(self.allocator);
        self.* = undefined;
    }

    /// Remember a host, ignoring case, and return its index.
    pub fn add(self: *List, host: []const u8) !usize {
        for (self.hosts.items, 0..) |known, index| {
            if (std.ascii.eqlIgnoreCase(known, host)) return index;
        }

        // An explicit host line wins over a stale `#hidden` marker, e.g. when
        // the player types the address again or edits the file by hand.
        self.forgetHidden(host);

        const copy = try self.allocator.dupe(u8, host);
        errdefer self.allocator.free(copy);
        try self.hosts.append(self.allocator, copy);
        return self.hosts.items.len - 1;
    }

    /// Make `host` the active realm, adding it to the end of the list if new.
    pub fn setActive(self: *List, host: []const u8) !usize {
        const index = try self.add(host);
        self.active = index;
        self.has_active = true;
        return index;
    }

    pub fn activeHost(self: *const List) []const u8 {
        if (self.hosts.items.len == 0) return "";
        return self.hosts.items[self.active];
    }

    /// Forget the realm at `index`. Removing a built-in realm also hides it, so
    /// the next start does not re-add it. The last realm cannot be removed: the
    /// client needs one to connect to.
    pub fn remove(self: *List, index: usize) error{ NotFound, LastRealm, OutOfMemory }!void {
        if (index >= self.hosts.items.len) return error.NotFound;
        if (self.hosts.items.len == 1) return error.LastRealm;

        const host = self.hosts.items[index];
        if (isPreset(host)) try self.hide(host);

        self.allocator.free(host);
        _ = self.hosts.orderedRemove(index);

        if (self.active == index) {
            self.active = @min(index, self.hosts.items.len - 1);
            self.has_active = true;
        } else if (self.active > index) {
            self.active -= 1;
        }
    }

    /// Point the realm at `index` at `host` instead and make it active. A
    /// renamed built-in realm hides its old address (like `remove`), otherwise
    /// `ensurePresets` would re-add it on the next start. Renaming onto a host
    /// that is already in the list is refused rather than merged.
    pub fn rename(
        self: *List,
        index: usize,
        host: []const u8,
    ) error{ NotFound, Duplicate, OutOfMemory }!void {
        if (index >= self.hosts.items.len) return error.NotFound;

        const old = self.hosts.items[index];
        if (std.ascii.eqlIgnoreCase(old, host)) {
            self.active = index;
            self.has_active = true;
            return;
        }

        for (self.hosts.items, 0..) |known, other| {
            if (other != index and std.ascii.eqlIgnoreCase(known, host)) return error.Duplicate;
        }

        const copy = try self.allocator.dupe(u8, host);
        errdefer self.allocator.free(copy);

        if (isPreset(old)) try self.hide(old);
        self.forgetHidden(host);

        self.allocator.free(old);
        self.hosts.items[index] = copy;
        self.active = index;
        self.has_active = true;
    }

    /// Append the built-in realms that the file history does not contain yet,
    /// skipping the ones the player removed.
    pub fn ensurePresets(self: *List) !void {
        for (presets) |preset| {
            if (self.isHidden(preset.host)) continue;
            _ = try self.add(preset.host);
        }
    }

    /// Parse `set realmlist` lines. All hosts are remembered in file order; the
    /// first uncommented line is the active realm.
    pub fn parse(allocator: std.mem.Allocator, text: []const u8) !List {
        var list = List.init(allocator);
        errdefer list.deinit();

        var lines = std.mem.splitScalar(u8, text, '\n');
        while (lines.next()) |line| {
            if (parseHiddenLine(line)) |host| {
                try list.hide(host);
                continue;
            }

            const entry = parseLine(line) orelse continue;
            const index = try list.add(entry.host);
            if (!entry.commented and !list.has_active) {
                list.active = index;
                list.has_active = true;
            }
        }

        list.pruneHidden();
        return list;
    }

    /// Render the file: one active line, every other remembered host commented
    /// out, then the hidden built-in realms. Allocated with `allocator`.
    pub fn render(self: *const List, allocator: std.mem.Allocator) ![]u8 {
        var out: std.Io.Writer.Allocating = .init(allocator);
        errdefer out.deinit();
        const writer = &out.writer;

        if (self.hosts.items.len > 0) {
            try writer.print("set realmlist {s}\n", .{self.activeHost()});
            for (self.hosts.items, 0..) |host, index| {
                if (index == self.active) continue;
                try writer.print("#set realmlist {s}\n", .{host});
            }
        }
        for (self.hidden.items) |host| {
            try writer.print("#hidden {s}\n", .{host});
        }
        return out.toOwnedSlice();
    }

    fn hide(self: *List, host: []const u8) !void {
        if (self.isHidden(host)) return;

        const copy = try self.allocator.dupe(u8, host);
        errdefer self.allocator.free(copy);
        try self.hidden.append(self.allocator, copy);
    }

    fn unhide(self: *List, host: []const u8) void {
        self.forgetHidden(host);
    }

    /// Drop hidden markers for hosts that are in the list; a `set realmlist`
    /// line is the player's explicit wish, whatever the file order.
    fn pruneHidden(self: *List) void {
        for (self.hosts.items) |host| self.forgetHidden(host);
    }

    fn forgetHidden(self: *List, host: []const u8) void {
        var index: usize = 0;
        while (index < self.hidden.items.len) {
            if (std.ascii.eqlIgnoreCase(self.hidden.items[index], host)) {
                self.allocator.free(self.hidden.items[index]);
                _ = self.hidden.orderedRemove(index);
            } else {
                index += 1;
            }
        }
    }

    fn isHidden(self: *const List, host: []const u8) bool {
        for (self.hidden.items) |known| {
            if (std.ascii.eqlIgnoreCase(known, host)) return true;
        }
        return false;
    }
};

const Line = struct {
    host: []const u8,
    commented: bool,
};

/// Recognise `set realmlist <host>` with optional leading `#` and whitespace.
fn parseLine(line: []const u8) ?Line {
    var rest = std.mem.trim(u8, line, " \t\r");

    var commented = false;
    if (rest.len > 0 and rest[0] == '#') {
        commented = true;
        rest = std.mem.trimStart(u8, rest[1..], " \t");
    }

    if (!std.ascii.startsWithIgnoreCase(rest, "set")) return null;
    if (rest.len == 3 or !std.ascii.isWhitespace(rest[3])) return null;
    rest = std.mem.trimStart(u8, rest[3..], " \t");

    if (!std.ascii.startsWithIgnoreCase(rest, "realmlist")) return null;
    if (rest.len == "realmlist".len or !std.ascii.isWhitespace(rest["realmlist".len])) return null;
    rest = std.mem.trimStart(u8, rest["realmlist".len..], " \t");

    // The client ignores everything after the address; accept trailing
    // whitespace and comments instead of folding them into the host.
    const end = std.mem.findAny(u8, rest, " \t#") orelse rest.len;
    if (end == 0) return null;

    return .{ .host = rest[0..end], .commented = commented };
}

/// Recognise the launcher's `#hidden <host>` marker, which records a built-in
/// realm the player removed. Any other comment is ignored, by us and the client.
fn parseHiddenLine(line: []const u8) ?[]const u8 {
    var rest = std.mem.trim(u8, line, " \t\r");
    if (rest.len == 0 or rest[0] != '#') return null;
    rest = std.mem.trimStart(u8, rest[1..], " \t");

    if (!std.ascii.startsWithIgnoreCase(rest, "hidden")) return null;
    if (rest.len == "hidden".len or !std.ascii.isWhitespace(rest["hidden".len])) return null;
    rest = std.mem.trimStart(u8, rest["hidden".len..], " \t");

    const end = std.mem.findAny(u8, rest, " \t#") orelse rest.len;
    if (end == 0) return null;
    return rest[0..end];
}

/// Trim a user-entered realm and reject anything that is not a plain host name
/// or IPv4 address. Returns the trimmed slice or null.
pub fn validate(text: []const u8) ?[]const u8 {
    const host = std.mem.trim(u8, text, " \t\r\n");
    if (host.len == 0 or host.len > max_host_len) return null;
    if (host[0] == '.' or host[0] == '-') return null;

    for (host) |ch| {
        if (!std.ascii.isAlphanumeric(ch) and ch != '.' and ch != '-') return null;
    }
    return host;
}

/// Whether `host` is one of the built-in realms.
pub fn isPreset(host: []const u8) bool {
    for (presets) |preset| {
        if (std.ascii.eqlIgnoreCase(preset.host, host)) return true;
    }
    return false;
}

/// Friendly name for the realm label: the preset name, or "Custom realm" for a
/// host the launcher does not recognise.
pub fn displayName(host: []const u8) []const u8 {
    for (presets) |preset| {
        if (std.ascii.eqlIgnoreCase(preset.host, host)) return preset.name;
    }
    return "Custom realm";
}

/// The realmlist files of one client installation, resolved once at startup.
///
/// `dir` is borrowed: the caller opens and closes the game directory, so the
/// same handle can serve `Wow.exe` checks and tests can pass a temporary
/// directory instead of a path.
pub const Client = struct {
    dir: std.Io.Dir,
    allocator: std.mem.Allocator,
    /// Paths relative to `dir`, owned. Existing files first; when the client
    /// has none yet, the one `write` should create.
    paths: std.ArrayListUnmanaged([]u8) = .empty,

    pub fn init(io: std.Io, allocator: std.mem.Allocator, dir: std.Io.Dir) !Client {
        var self = Client{ .dir = dir, .allocator = allocator };
        errdefer self.freePaths();

        var locales: std.ArrayListUnmanaged([]u8) = .empty;
        defer {
            for (locales.items) |name| allocator.free(name);
            locales.deinit(allocator);
        }

        const data: ?std.Io.Dir = dir.openDir(io, "Data", .{ .iterate = true }) catch |err| switch (err) {
            error.FileNotFound, error.NotDir => null,
            else => return err,
        };
        defer if (data) |open| open.close(io);

        if (data) |open| {
            var iterator = open.iterate();
            while (try iterator.next(io)) |entry| {
                if (entry.kind != .directory) continue;
                try locales.append(allocator, try allocator.dupe(u8, entry.name));
            }
        }

        std.mem.sort([]u8, locales.items, {}, struct {
            fn lessThan(_: void, a: []u8, b: []u8) bool {
                return std.mem.lessThan(u8, a, b);
            }
        }.lessThan);

        if (data) |open| {
            for (locales.items) |locale| {
                const file = try std.fs.path.join(allocator, &.{ locale, "realmlist.wtf" });
                defer allocator.free(file);
                if (!exists(open, io, file)) continue;

                try self.paths.append(
                    allocator,
                    try std.fs.path.join(allocator, &.{ "Data", locale, "realmlist.wtf" }),
                );
            }
        }

        // Pre-1.9 clients read a realmlist at the game root. Only used when the
        // modern Data/<locale> files are absent, so it cannot shadow them.
        if (self.paths.items.len == 0 and exists(dir, io, "realmlist.wtf")) {
            try self.paths.append(allocator, try allocator.dupe(u8, "realmlist.wtf"));
        }

        if (self.paths.items.len == 0) {
            const locale = try self.chooseLocale(io, locales.items);
            defer allocator.free(locale);
            try self.paths.append(
                allocator,
                try std.fs.path.join(allocator, &.{ "Data", locale, "realmlist.wtf" }),
            );
        }

        return self;
    }

    /// Free the paths. The game directory handle stays with the caller.
    pub fn deinit(self: *Client) void {
        self.freePaths();
        self.* = undefined;
    }

    fn freePaths(self: *Client) void {
        for (self.paths.items) |path| self.allocator.free(path);
        self.paths.deinit(self.allocator);
        self.paths = .empty;
    }

    /// The file shown in the UI and used to seed the realm list.
    pub fn primary(self: *const Client) []const u8 {
        if (self.paths.items.len == 0) return "Data/enUS/realmlist.wtf";
        return self.paths.items[0];
    }

    /// Read every realmlist file and merge their realms. Read failures other
    /// than out-of-memory are treated as an absent file: the launcher can still
    /// offer the presets, and `write` reports the real error if it cannot save.
    pub fn load(self: *Client, io: std.Io) !List {
        var list = List.init(self.allocator);
        errdefer list.deinit();

        for (self.paths.items) |path| {
            const text = self.dir.readFileAlloc(io, path, self.allocator, max_file_size) catch |err| switch (err) {
                error.OutOfMemory => return error.OutOfMemory,
                else => continue,
            };
            defer self.allocator.free(text);

            var parsed = try List.parse(self.allocator, text);
            defer parsed.deinit();

            for (parsed.hosts.items) |host| _ = try list.add(host);
            for (parsed.hidden.items) |host| try list.hide(host);
            if (parsed.has_active and !list.has_active) {
                list.active = try list.add(parsed.hosts.items[parsed.active]);
                list.has_active = true;
            }
        }

        // A host seen in any file wins over a `#hidden` marker from another.
        list.pruneHidden();
        return list;
    }

    /// Write the realm list to every realmlist file of the installation,
    /// creating `Data/<locale>` (and the file) when the client has none yet.
    pub fn write(self: *Client, io: std.Io, list: *const List) !void {
        const text = try list.render(self.allocator);
        defer self.allocator.free(text);

        for (self.paths.items) |path| {
            if (std.fs.path.dirname(path)) |parent| {
                try self.dir.createDirPath(io, parent);
            }
            try self.dir.writeFile(io, .{ .sub_path = path, .data = text });
        }
    }

    /// The locale directory of the client: `SET locale "xxYY"` from
    /// `WTF/Config.wtf`, else the first `Data/<xxYY>` directory, else enUS.
    /// Allocated with the client allocator.
    fn chooseLocale(self: *Client, io: std.Io, locales: []const []u8) ![]u8 {
        if (try self.configuredLocale(io)) |locale| {
            defer self.allocator.free(locale);
            if (locales.len == 0) return self.allocator.dupe(u8, locale);
            for (locales) |candidate| {
                if (std.ascii.eqlIgnoreCase(candidate, locale)) {
                    return self.allocator.dupe(u8, candidate);
                }
            }
        }

        for (locales) |candidate| {
            if (isLocaleName(candidate)) return self.allocator.dupe(u8, candidate);
        }

        return self.allocator.dupe(u8, "enUS");
    }

    fn configuredLocale(self: *Client, io: std.Io) !?[]u8 {
        const text = self.dir.readFileAlloc(
            io,
            "WTF/Config.wtf",
            self.allocator,
            max_file_size,
        ) catch |err| switch (err) {
            error.OutOfMemory => return error.OutOfMemory,
            else => return null,
        };
        defer self.allocator.free(text);

        var lines = std.mem.splitScalar(u8, text, '\n');
        while (lines.next()) |line| {
            const trimmed = std.mem.trim(u8, line, " \t\r");
            if (!std.ascii.startsWithIgnoreCase(trimmed, "set")) continue;
            if (trimmed.len == 3 or !std.ascii.isWhitespace(trimmed[3])) continue;

            var rest = std.mem.trimStart(u8, trimmed[3..], " \t");
            if (!std.ascii.startsWithIgnoreCase(rest, "locale")) continue;
            rest = std.mem.trimStart(u8, rest["locale".len..], " \t");

            const open = std.mem.findScalar(u8, rest, '"') orelse continue;
            const close = std.mem.findScalarPos(u8, rest, open + 1, '"') orelse continue;
            const value = std.mem.trim(u8, rest[open + 1 .. close], " \t");
            if (!isLocaleName(value)) continue;

            return try self.allocator.dupe(u8, value);
        }

        return null;
    }
};

/// `enUS`, `deDE`, ...: four ASCII letters/digits, the shape the client uses
/// for its locale directory.
fn isLocaleName(name: []const u8) bool {
    if (name.len != 4) return false;
    for (name) |ch| {
        if (!std.ascii.isAlphanumeric(ch)) return false;
    }
    return true;
}

fn exists(dir: std.Io.Dir, io: std.Io, path: []const u8) bool {
    dir.access(io, path, .{}) catch return false;
    return true;
}

test "parse keeps history and finds the active realm" {
    const text =
        "set realmlist logon.arenacraft.fun\n" ++
        "#set realmlist logon.warmane.com\n" ++
        "#set realmlist 62.83.16.59\n" ++
        "#set realmlist logon.arenacraft.fun\n" ++
        "#set realmlist 157.90.144.252\n";

    var list = try List.parse(std.testing.allocator, text);
    defer list.deinit();

    try std.testing.expectEqual(@as(usize, 4), list.hosts.items.len);
    try std.testing.expectEqual(@as(usize, 0), list.active);
    try std.testing.expectEqualStrings("logon.arenacraft.fun", list.activeHost());
    try std.testing.expectEqualStrings("logon.warmane.com", list.hosts.items[1]);
    try std.testing.expectEqualStrings("62.83.16.59", list.hosts.items[2]);
    try std.testing.expectEqualStrings("157.90.144.252", list.hosts.items[3]);
}

test "parse finds the first uncommented line and tolerates CRLF and comments" {
    const text =
        "SET REALMLIST logon.warmane.com\r\n" ++
        "  # set realmlist  62.83.16.59  \r\n" ++
        "garbage\r\n" ++
        "set realmlist other.example\n";

    var list = try List.parse(std.testing.allocator, text);
    defer list.deinit();

    try std.testing.expect(list.has_active);
    try std.testing.expectEqualStrings("logon.warmane.com", list.activeHost());
    try std.testing.expectEqual(@as(usize, 3), list.hosts.items.len);
}

test "render activates the chosen realm and comments the rest" {
    var list = List.init(std.testing.allocator);
    defer list.deinit();

    _ = try list.add("logon.arenacraft.fun");
    _ = try list.add("logon.warmane.com");
    _ = try list.add("62.83.16.59");
    _ = try list.setActive("62.83.16.59");

    const text = try list.render(std.testing.allocator);
    defer std.testing.allocator.free(text);

    try std.testing.expectEqualStrings(
        "set realmlist 62.83.16.59\n" ++
            "#set realmlist logon.arenacraft.fun\n" ++
            "#set realmlist logon.warmane.com\n",
        text,
    );
}

test "add and setActive are case insensitive" {
    var list = List.init(std.testing.allocator);
    defer list.deinit();

    _ = try list.add("logon.arenacraft.fun");
    const index = try list.setActive("LOGON.ARENACRAFT.FUN");

    try std.testing.expectEqual(@as(usize, 1), list.hosts.items.len);
    try std.testing.expectEqual(@as(usize, 0), index);
    try std.testing.expectEqualStrings("logon.arenacraft.fun", list.activeHost());
}

test "remove drops a realm and keeps the active realm valid" {
    var list = List.init(std.testing.allocator);
    defer list.deinit();

    _ = try list.add("logon.arenacraft.fun");
    _ = try list.add("62.83.16.59");
    _ = try list.add("logon.warmane.com");
    _ = try list.setActive("62.83.16.59");

    // Removing the active custom realm activates the one that takes its place
    // and needs no hidden marker.
    try list.remove(1);
    try std.testing.expectEqual(@as(usize, 2), list.hosts.items.len);
    try std.testing.expectEqual(@as(usize, 1), list.active);
    try std.testing.expectEqualStrings("logon.warmane.com", list.activeHost());
    try std.testing.expectEqual(@as(usize, 0), list.hidden.items.len);

    // Removing a preset records it as hidden.
    try list.remove(1);
    try std.testing.expectEqualStrings("logon.arenacraft.fun", list.activeHost());
    try std.testing.expectEqual(@as(usize, 1), list.hidden.items.len);
}

test "removing a preset hides it across restarts" {
    var list = List.init(std.testing.allocator);
    defer list.deinit();

    _ = try list.add("logon.arenacraft.fun");
    _ = try list.add("logon.warmane.com");
    _ = try list.setActive("logon.warmane.com");

    try list.remove(1);

    const text = try list.render(std.testing.allocator);
    defer std.testing.allocator.free(text);
    try std.testing.expectEqualStrings(
        "set realmlist logon.arenacraft.fun\n" ++
            "#hidden logon.warmane.com\n",
        text,
    );

    var reread = try List.parse(std.testing.allocator, text);
    defer reread.deinit();
    try reread.ensurePresets();

    try std.testing.expectEqual(@as(usize, 1), reread.hosts.items.len);
    try std.testing.expectEqualStrings("logon.arenacraft.fun", reread.activeHost());
}

test "typing a hidden preset again brings it back" {
    var list = try List.parse(
        std.testing.allocator,
        "#hidden logon.warmane.com\nset realmlist logon.arenacraft.fun\n",
    );
    defer list.deinit();
    try list.ensurePresets();
    try std.testing.expectEqual(@as(usize, 1), list.hosts.items.len);

    _ = try list.setActive("logon.warmane.com");
    try std.testing.expectEqual(@as(usize, 0), list.hidden.items.len);
    try std.testing.expectEqual(@as(usize, 2), list.hosts.items.len);
}

test "the last realm cannot be removed" {
    var list = List.init(std.testing.allocator);
    defer list.deinit();

    _ = try list.setActive("127.0.0.1");
    try std.testing.expectError(error.LastRealm, list.remove(0));
    try std.testing.expectEqual(@as(usize, 1), list.hosts.items.len);
}

test "rename re-addresses a realm and makes it active" {
    var list = List.init(std.testing.allocator);
    defer list.deinit();

    _ = try list.add("logon.arenacraft.fun");
    _ = try list.add("logon.warmane.com");
    _ = try list.setActive("logon.arenacraft.fun");

    try list.rename(1, "127.0.0.1");
    try std.testing.expectEqual(@as(usize, 1), list.active);
    try std.testing.expectEqualStrings("127.0.0.1", list.activeHost());
    try std.testing.expectEqualStrings("logon.arenacraft.fun", list.hosts.items[0]);
    // The row was a built-in realm, so its old address is hidden.
    try std.testing.expectEqual(@as(usize, 1), list.hidden.items.len);
    try std.testing.expectEqualStrings("logon.warmane.com", list.hidden.items[0]);
}

test "rename refuses a host that is already in the list" {
    var list = List.init(std.testing.allocator);
    defer list.deinit();

    _ = try list.add("logon.arenacraft.fun");
    _ = try list.add("logon.warmane.com");

    try std.testing.expectError(error.Duplicate, list.rename(0, "LOGON.WARMANE.COM"));
    try std.testing.expectEqual(@as(usize, 2), list.hosts.items.len);
}

test "renaming a preset hides its old address across restarts" {
    var list = List.init(std.testing.allocator);
    defer list.deinit();

    _ = try list.add("logon.arenacraft.fun");
    _ = try list.add("logon.warmane.com");
    _ = try list.setActive("logon.arenacraft.fun");

    try list.rename(0, "logon.example.com");

    const text = try list.render(std.testing.allocator);
    defer std.testing.allocator.free(text);
    try std.testing.expectEqualStrings(
        "set realmlist logon.example.com\n" ++
            "#set realmlist logon.warmane.com\n" ++
            "#hidden logon.arenacraft.fun\n",
        text,
    );

    var reread = try List.parse(std.testing.allocator, text);
    defer reread.deinit();
    try reread.ensurePresets();
    try std.testing.expectEqual(@as(usize, 2), reread.hosts.items.len);
    try std.testing.expectEqualStrings("logon.example.com", reread.activeHost());
}

test "validate accepts hosts and IPv4, rejects file syntax" {
    try std.testing.expectEqualStrings("logon.arenacraft.fun", validate("  logon.arenacraft.fun \r\n").?);
    try std.testing.expectEqualStrings("62.83.16.59", validate("62.83.16.59").?);

    try std.testing.expect(validate("") == null);
    try std.testing.expect(validate("   ") == null);
    try std.testing.expect(validate("logon.arenacraft.fun:3724") == null);
    try std.testing.expect(validate("set realmlist wow") == null);
    try std.testing.expect(validate("#comment") == null);
    try std.testing.expect(validate("../evil") == null);
}

test "displayName maps presets and falls back for custom realms" {
    try std.testing.expectEqualStrings("Arenacraft", displayName("logon.arenacraft.fun"));
    try std.testing.expectEqualStrings("Warmane", displayName("LOGON.WARMANE.COM"));
    try std.testing.expectEqualStrings("Custom realm", displayName("127.0.0.1"));
}

test "client discovers Data/<locale>/realmlist.wtf and rewrites it" {
    var tmp = std.testing.tmpDir(.{ .iterate = true });
    defer tmp.cleanup();

    try tmp.dir.createDirPath(std.testing.io, "Data/enUS");
    try tmp.dir.writeFile(std.testing.io, .{
        .sub_path = "Data/enUS/realmlist.wtf",
        .data = "set realmlist logon.arenacraft.fun\n#set realmlist logon.warmane.com\n",
    });

    var client = try Client.init(std.testing.io, std.testing.allocator, tmp.dir);
    defer client.deinit();

    const expected = try std.fs.path.join(std.testing.allocator, &.{ "Data", "enUS", "realmlist.wtf" });
    defer std.testing.allocator.free(expected);

    try std.testing.expectEqual(@as(usize, 1), client.paths.items.len);
    try std.testing.expectEqualStrings(expected, client.primary());

    var list = try client.load(std.testing.io);
    defer list.deinit();
    try list.ensurePresets();
    _ = try list.setActive("62.83.16.59");
    try client.write(std.testing.io, &list);

    const written = try tmp.dir.readFileAlloc(
        std.testing.io,
        "Data/enUS/realmlist.wtf",
        std.testing.allocator,
        max_file_size,
    );
    defer std.testing.allocator.free(written);

    try std.testing.expectEqualStrings(
        "set realmlist 62.83.16.59\n" ++
            "#set realmlist logon.arenacraft.fun\n" ++
            "#set realmlist logon.warmane.com\n",
        written,
    );
}

test "client creates the locale file when the client has none" {
    var tmp = std.testing.tmpDir(.{ .iterate = true });
    defer tmp.cleanup();

    try tmp.dir.createDirPath(std.testing.io, "Data/deDE");
    try tmp.dir.createDirPath(std.testing.io, "WTF");
    try tmp.dir.writeFile(std.testing.io, .{
        .sub_path = "WTF/Config.wtf",
        .data = "SET locale \"deDE\"\nSET gxWindow \"1\"\n",
    });

    var client = try Client.init(std.testing.io, std.testing.allocator, tmp.dir);
    defer client.deinit();

    const expected = try std.fs.path.join(std.testing.allocator, &.{ "Data", "deDE", "realmlist.wtf" });
    defer std.testing.allocator.free(expected);

    try std.testing.expectEqualStrings(expected, client.primary());

    var list = List.init(std.testing.allocator);
    defer list.deinit();
    _ = try list.setActive("logon.arenacraft.fun");
    try client.write(std.testing.io, &list);

    const written = try tmp.dir.readFileAlloc(
        std.testing.io,
        "Data/deDE/realmlist.wtf",
        std.testing.allocator,
        max_file_size,
    );
    defer std.testing.allocator.free(written);
    try std.testing.expectEqualStrings("set realmlist logon.arenacraft.fun\n", written);
}

test "client falls back to enUS without a client config" {
    var tmp = std.testing.tmpDir(.{ .iterate = true });
    defer tmp.cleanup();

    var client = try Client.init(std.testing.io, std.testing.allocator, tmp.dir);
    defer client.deinit();

    const expected = try std.fs.path.join(std.testing.allocator, &.{ "Data", "enUS", "realmlist.wtf" });
    defer std.testing.allocator.free(expected);

    try std.testing.expectEqualStrings(expected, client.primary());
}
