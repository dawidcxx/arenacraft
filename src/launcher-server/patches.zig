//! Patch archives built once when `ac launcher-server` starts.
//!
//! * `patch-D.mpq` contains the whitelisted DBCs from `<DataDir>/dbc`, stored
//!   under `DBFilesClient\...` where the client looks for them.
//! * `patch-R.mpq` contains every regular file below `<AssetsDir>`, mirroring
//!   the directory tree at the archive root.
//!
//! Both archives are kept in memory (the launcher downloads them through
//! `get_patch`) and embed an archive-specific manifest at the root:
//! `patch-D.sum.txt` / `patch-R.sum.txt`. A manifest line is
//! `<md5>  <archive path>` and covers the *input* files only; the manifest
//! itself is not listed.

const std = @import("std");

const mpq = @import("mpq.zig");

/// DBCs offered to clients. Patch archives do not replace the client's whole
/// DBC set, so every file that should be visible has to be listed here.
pub const dbc_files = [_][]const u8{
    "Spell.dbc",
};

pub const Options = struct {
    data_dir: []const u8,
    assets_dir: []const u8,
};

pub const Archive = struct {
    /// `D` or `R`; accepts `patch-D` / `patch-R` too.
    key: u8,
    filename: []const u8,
    manifest_name: []const u8,
    bytes: []const u8,
    manifest: []const u8,
    md5: [16]u8,
};

pub const Patches = struct {
    allocator: std.mem.Allocator,
    archives: [2]Archive,

    pub fn init(io: std.Io, allocator: std.mem.Allocator, options: Options) !Patches {
        var result: Patches = .{
            .allocator = allocator,
            .archives = undefined,
        };
        result.archives[0] = try buildD(io, allocator, options.data_dir);
        errdefer allocator.free(result.archives[0].bytes);
        errdefer allocator.free(result.archives[0].manifest);

        result.archives[1] = try buildR(io, allocator, options.assets_dir);
        errdefer allocator.free(result.archives[1].bytes);
        errdefer allocator.free(result.archives[1].manifest);

        return result;
    }

    pub fn deinit(self: *Patches) void {
        for (self.archives) |archive| {
            self.allocator.free(archive.bytes);
            self.allocator.free(archive.manifest);
        }
        self.* = undefined;
    }

    /// Resolves `D`, `R`, `patch-D` or `patch-R` case-insensitively.
    pub fn byName(self: *const Patches, name: []const u8) ?*const Archive {
        for (&self.archives) |*archive| {
            const key = archive.key;
            const short: [1]u8 = .{key};
            var buffer: [16]u8 = undefined;
            const patch_name = std.fmt.bufPrint(&buffer, "patch-{c}", .{key}) catch unreachable;
            if (std.ascii.eqlIgnoreCase(name, short[0..])) return archive;
            if (std.ascii.eqlIgnoreCase(name, patch_name)) return archive;
            if (std.ascii.eqlIgnoreCase(name, archive.filename)) return archive;
        }
        return null;
    }
};

fn buildD(io: std.Io, allocator: std.mem.Allocator, data_dir: []const u8) !Archive {
    std.log.info("launcher-server: building patch-D.mpq from {s}/dbc", .{data_dir});

    var arena = std.heap.ArenaAllocator.init(allocator);
    defer arena.deinit();
    const scratch = arena.allocator();

    const data = openDir(io, data_dir, .{}) catch |err| {
        std.log.err("patch-D: cannot open data directory '{s}': {s}", .{ data_dir, @errorName(err) });
        return err;
    };
    defer data.close(io);

    const dbc = data.openDir(io, "dbc", .{}) catch |err| {
        std.log.err("patch-D: cannot open '{s}/dbc': {s}", .{ data_dir, @errorName(err) });
        return err;
    };
    defer dbc.close(io);

    var files: std.ArrayList(mpq.File) = .empty;
    defer files.deinit(scratch);

    for (dbc_files) |name| {
        const bytes = dbc.readFileAlloc(io, name, scratch, .unlimited) catch |err| {
            std.log.err("patch-D: cannot read '{s}/dbc/{s}': {s}", .{ data_dir, name, @errorName(err) });
            return err;
        };
        const path = try std.fmt.allocPrint(scratch, "DBFilesClient\\{s}", .{name});
        try files.append(scratch, .{ .path = path, .data = bytes });
    }

    return finishArchive(allocator, scratch, .{
        .key = 'D',
        .filename = "patch-D.mpq",
        .manifest_name = "patch-D.sum.txt",
    }, files.items);
}

fn buildR(io: std.Io, allocator: std.mem.Allocator, assets_dir: []const u8) !Archive {
    std.log.info("launcher-server: building patch-R.mpq from {s}", .{assets_dir});

    var arena = std.heap.ArenaAllocator.init(allocator);
    defer arena.deinit();
    const scratch = arena.allocator();

    const root = openDir(io, assets_dir, .{ .iterate = true }) catch |err| {
        std.log.err("patch-R: cannot open assets directory '{s}': {s}", .{ assets_dir, @errorName(err) });
        return err;
    };
    defer root.close(io);

    var files: std.ArrayList(mpq.File) = .empty;
    defer files.deinit(scratch);

    var walker = try root.walk(scratch);
    defer walker.deinit();
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file) continue;

        const path = try toArchivePath(scratch, entry.path);
        const bytes = entry.dir.readFileAlloc(io, entry.basename, scratch, .unlimited) catch |err| {
            std.log.err("patch-R: cannot read '{s}': {s}", .{ entry.path, @errorName(err) });
            return err;
        };
        try files.append(scratch, .{ .path = path, .data = bytes });
    }

    return finishArchive(allocator, scratch, .{
        .key = 'R',
        .filename = "patch-R.mpq",
        .manifest_name = "patch-R.sum.txt",
    }, files.items);
}

const Identity = struct {
    key: u8,
    filename: []const u8,
    manifest_name: []const u8,
};

fn finishArchive(
    allocator: std.mem.Allocator,
    scratch: std.mem.Allocator,
    identity: Identity,
    input_files: []mpq.File,
) !Archive {
    // Deterministic manifests and archive layout regardless of walk order.
    std.mem.sort(mpq.File, input_files, {}, struct {
        fn lessThan(_: void, a: mpq.File, b: mpq.File) bool {
            return std.mem.order(u8, a.path, b.path) == .lt;
        }
    }.lessThan);

    // With no inputs (an empty assets dir) `mpq.build` would make a valid but
    // useless archive; the manifest is still worth serving, so keep it.
    const manifest = try buildManifest(allocator, input_files);
    errdefer allocator.free(manifest);

    var all: std.ArrayList(mpq.File) = .empty;
    defer all.deinit(scratch);
    try all.appendSlice(scratch, input_files);
    try all.append(scratch, .{ .path = identity.manifest_name, .data = manifest });

    const bytes = try mpq.build(allocator, all.items);
    errdefer allocator.free(bytes);

    var md5: [16]u8 = undefined;
    std.crypto.hash.Md5.hash(bytes, &md5, .{});

    std.log.info(
        "launcher-server: built {s} ({d} input files, {d} bytes, md5 {s})",
        .{ identity.filename, input_files.len, bytes.len, std.fmt.bytesToHex(md5, .lower) },
    );

    return .{
        .key = identity.key,
        .filename = identity.filename,
        .manifest_name = identity.manifest_name,
        .bytes = bytes,
        .manifest = manifest,
        .md5 = md5,
    };
}

/// `files` must already be sorted by path.
fn buildManifest(allocator: std.mem.Allocator, files: []const mpq.File) ![]u8 {
    var out = try std.Io.Writer.Allocating.initCapacity(allocator, 4096);
    defer out.deinit();

    for (files) |file| {
        var digest: [16]u8 = undefined;
        std.crypto.hash.Md5.hash(file.data, &digest, .{});
        try out.writer.print("{s}  {s}\n", .{ std.fmt.bytesToHex(digest, .lower), file.path });
    }

    var list = out.toArrayList();
    errdefer list.deinit(allocator);
    return list.toOwnedSlice(allocator);
}

fn toArchivePath(allocator: std.mem.Allocator, path: []const u8) ![]u8 {
    const out = try allocator.dupe(u8, path);
    for (out) |*byte| {
        if (byte.* == '/') byte.* = '\\';
    }
    if (!mpq.isArchivePath(out)) {
        std.log.err("patch-R: '{s}' is not a valid archive path", .{path});
        return error.InvalidPath;
    }
    return out;
}

fn openDir(io: std.Io, path: []const u8, options: std.Io.Dir.OpenOptions) !std.Io.Dir {
    if (std.fs.path.isAbsolute(path)) return std.Io.Dir.openDirAbsolute(io, path, options);
    return std.Io.Dir.cwd().openDir(io, path, options);
}

test "patch manifests are sorted md5 lines" {
    const allocator = std.testing.allocator;
    const files = [_]mpq.File{
        .{ .path = "a.txt", .data = "a" },
        .{ .path = "b.txt", .data = "b" },
    };
    const manifest = try buildManifest(allocator, &files);
    defer allocator.free(manifest);

    // MD5("a") = 0cc175b9c0f1b6a831c399e269772661, MD5("b") = 92eb5ffee6ae2fec3ad71c777531578f
    try std.testing.expectEqualStrings(
        "0cc175b9c0f1b6a831c399e269772661  a.txt\n" ++
            "92eb5ffee6ae2fec3ad71c777531578f  b.txt\n",
        manifest,
    );
}

test "patch archives can be built from a data and assets tree" {
    const allocator = std.testing.allocator;
    const io = std.testing.io;

    var tmp = std.testing.tmpDir(.{ .iterate = true });
    defer tmp.cleanup();

    try tmp.dir.createDirPath(io, "data/dbc");
    try tmp.dir.writeFile(io, .{ .sub_path = "data/dbc/Spell.dbc", .data = "spell data spell data spell data" });
    try tmp.dir.createDirPath(io, "assets/Interface");
    try tmp.dir.writeFile(io, .{ .sub_path = "assets/foo.txt", .data = "foo\n" });
    try tmp.dir.writeFile(io, .{ .sub_path = "assets/Interface/bar.txt", .data = "bar\n" });

    const data_path = try tmp.dir.realPathFileAlloc(io, "data", allocator);
    defer allocator.free(data_path);
    const assets_path = try tmp.dir.realPathFileAlloc(io, "assets", allocator);
    defer allocator.free(assets_path);

    var patches = try Patches.init(io, allocator, .{
        .data_dir = data_path,
        .assets_dir = assets_path,
    });
    defer patches.deinit();

    try std.testing.expect(patches.byName("D") != null);
    try std.testing.expect(patches.byName("patch-r") != null);
    try std.testing.expect(patches.byName("nope") == null);

    const d = patches.byName("D").?;
    try std.testing.expectEqualStrings(
        "bce4969c8bf47af65643f427ee041d27  DBFilesClient\\Spell.dbc\n",
        d.manifest,
    );
    try std.testing.expect(d.bytes.len > 32);

    const r = patches.byName("R").?;
    try std.testing.expect(std.mem.indexOf(u8, r.manifest, "foo.txt\n") != null);
    try std.testing.expect(std.mem.indexOf(u8, r.manifest, "Interface\\bar.txt\n") != null);
}

test "asset paths become backslash separated archive paths" {
    const allocator = std.testing.allocator;
    const path = try toArchivePath(allocator, "Interface/AddOns/foo.toc");
    defer allocator.free(path);
    try std.testing.expectEqualStrings("Interface\\AddOns\\foo.toc", path);
}
