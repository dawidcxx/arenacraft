// NOTE: Quick mockup, don't rely on this too much
// may leak memory

const std = @import("std");

pub const Ext = []const []const u8;
pub const JUST_CPP: Ext = &[_][]const u8{".cpp"};
pub const JUST_C: Ext = &[_][]const u8{".c"};
pub const JUST_CC: Ext = &[_][]const u8{".cc"};
pub const JUST_H: Ext = &[_][]const u8{".h"};

pub const SourceQueryOptions = struct {
    extensions: Ext,
    recursive: bool = false,
};

pub fn querySources(
    gpa: std.mem.Allocator,
    io: std.Io,
    dir: []const u8,
    options: SourceQueryOptions,
) SourceSet {
    var out: std.ArrayListUnmanaged([]const u8) = .empty;
    out.ensureTotalCapacity(gpa, 128) catch {
        @panic("OOM");
    };
    const target_dir = std.Io.Dir.cwd().openDir(io, dir, .{ .iterate = true }) catch |e| {
        std.debug.panic("Failed to openDir(), reason='{}'", .{e});
    };
    defer target_dir.close(io);
    // build paths relative to the working directory, so strip a leading "./"
    const clean_dir = if (std.mem.startsWith(u8, dir, "./")) dir[2..] else dir;
    internalScanFilesRecursive(gpa, io, target_dir, clean_dir, &out, options) catch |e| {
        std.debug.panic("Failed to query sources recursively, reason='{}'", .{e});
    };
    return .{
        .srcs = out,
        .alloc = gpa,
        .base_dir = dir,
    };
}

pub const SourceSet = struct {
    base_dir: []const u8,
    srcs: std.ArrayListUnmanaged([]const u8),
    alloc: std.mem.Allocator,

    pub fn get(self: SourceSet) []const []const u8 {
        return self.srcs.items;
    }

    pub fn with(self: SourceSet, io: std.Io, dir: []const u8, options: SourceQueryOptions) SourceSet {
        const other = querySources(self.alloc, io, dir, options);
        const current_copy = self.alloc.dupe([]const u8, self.srcs.items) catch {
            @panic("OOM");
        };
        const joined = std.mem.concat(self.alloc, []const u8, &.{ current_copy, other.srcs.items }) catch {
            @panic("OOM");
        };
        return .{
            .srcs = std.ArrayListUnmanaged([]const u8).fromOwnedSlice(joined),
            .base_dir = dir,
            .alloc = self.alloc,
        };
    }

    pub fn append(self: *SourceSet, path: []const u8) void {
        self.srcs.append(self.alloc, path) catch {
            @panic("OOM");
        };
    }

    pub fn filterOut(self: *SourceSet, file_name: []const u8) void {
        for (self.srcs.items, 0..) |src, i| {
            if (std.mem.endsWith(u8, src, file_name)) {
                _ = self.srcs.swapRemove(i);
                return;
            }
        }
    }

    pub fn filterOutByGlob(self: *SourceSet, glob: []const u8) void {
        for (self.srcs.items, 0..) |src, i| {
            if (internalMatchGlob(glob, src)) {
                _ = self.srcs.swapRemove(i);
            }
        }
    }

    pub fn debugPrint(self: SourceSet) void {
        std.debug.print("SourceSet contents of '{s}' ({} files):\n", .{ self.base_dir, self.srcs.items.len });
        // Create a copy of items for sorting
        const sorted_items = self.alloc.dupe([]const u8, self.srcs.items) catch {
            @panic("OOM");
        };
        defer self.alloc.free(sorted_items);

        // Sort by depth (number of '/' characters) then alphabetically
        std.sort.block([]const u8, sorted_items, {}, struct {
            fn lessThan(_: void, a: []const u8, b: []const u8) bool {
                const depth_a = std.mem.count(u8, a, "/") + (std.mem.count(u8, a, "./"));
                const depth_b = std.mem.count(u8, b, "/") + (std.mem.count(u8, b, "./"));
                if (depth_a != depth_b) {
                    return depth_a < depth_b;
                }
                return std.mem.lessThan(u8, a, b);
            }
        }.lessThan);

        for (sorted_items) |src| {
            const depth = std.mem.count(u8, src, "/");
            const max_depth = @min(depth, 10); // Limit max indent to prevent excessive nesting

            // Create indent string at runtime
            var indent_buf: [21]u8 = undefined; // 10 * 2 + 1 for null terminator
            var indent_len: usize = 0;
            for (0..max_depth) |_| {
                indent_buf[indent_len] = ' ';
                indent_buf[indent_len + 1] = ' ';
                indent_len += 2;
            }
            const indent = indent_buf[0..indent_len];

            std.debug.print("{s}  {s}\n", .{ indent, src });
        }
    }

    pub fn deinit(self: *SourceSet) void {
        self.srcs.deinit(self.alloc);
    }
};

//
// INTERNALS
//
fn internalScanFilesRecursive(
    gpa: std.mem.Allocator,
    io: std.Io,
    curr: std.Io.Dir,
    prefix: []const u8,
    output: *std.ArrayListUnmanaged([]const u8),
    options: SourceQueryOptions,
) !void {
    var it = curr.iterate();
    while (try it.next(io)) |entry| {
        const entry_path = try std.fs.path.join(gpa, &.{ prefix, entry.name });
        defer gpa.free(entry_path);

        if (entry.kind == .directory and options.recursive) {
            const child_dir = try curr.openDir(io, entry.name, .{ .iterate = true });
            defer child_dir.close(io);
            try internalScanFilesRecursive(gpa, io, child_dir, entry_path, output, options);
            continue;
        }

        for (options.extensions) |ext| {
            if (std.mem.endsWith(u8, entry.name, ext)) {
                const found_source_file = try gpa.dupe(u8, entry_path);
                try output.append(gpa, found_source_file);
            }
        }
    }
}

fn internalMatchGlob(pattern: []const u8, text: []const u8) bool {
    var pi: usize = 0; // pattern index
    var ti: usize = 0; // text index
    var star_idx: ?usize = null; // last '*' position in pattern
    var match_idx: usize = 0; // position in text after last '*' match

    while (ti < text.len) {
        if (pi < pattern.len and (pattern[pi] == text[ti] or pattern[pi] == '?')) {
            // Character match or '?' wildcard
            pi += 1;
            ti += 1;
        } else if (pi < pattern.len and pattern[pi] == '*') {
            // '*' wildcard - mark position and try matching rest
            star_idx = pi;
            match_idx = ti;
            pi += 1;
        } else if (star_idx) |star| {
            // No match, but we have a previous '*' - backtrack
            pi = star + 1;
            match_idx += 1;
            ti = match_idx;
        } else {
            // No match and no '*' to backtrack to
            return false;
        }
    }

    // Skip remaining '*' in pattern
    while (pi < pattern.len and pattern[pi] == '*') {
        pi += 1;
    }

    // Match succeeds if we've consumed entire pattern
    return pi == pattern.len;
}

test "internalMatchGlob" {
    // Exact match
    try std.testing.expect(internalMatchGlob("hello", "hello"));
    try std.testing.expect(!internalMatchGlob("hello", "world"));

    // Question mark
    try std.testing.expect(internalMatchGlob("h?llo", "hello"));

    // Star patterns
    try std.testing.expect(internalMatchGlob("*.txt", "file.txt"));
    try std.testing.expect(!internalMatchGlob("*.txt", "file.txt.bak"));
    try std.testing.expect(internalMatchGlob("test*", "test123"));
    try std.testing.expect(internalMatchGlob("*test*", "mytestfile"));

    // Multiple wildcards
    try std.testing.expect(internalMatchGlob("a*b*c", "aXXbYYc"));
    try std.testing.expect(internalMatchGlob("*.c?p", "file.cpp"));

    // Edge cases
    try std.testing.expect(internalMatchGlob("*", "anything"));
    try std.testing.expect(internalMatchGlob("", ""));
    try std.testing.expect(!internalMatchGlob("", "a"));

    // Standard usage
    try std.testing.expect(internalMatchGlob("*/**.cpp", "/foo/bar/daz/xdd.cpp"));
    try std.testing.expect(internalMatchGlob("*.cpp", "any.cpp"));
    try std.testing.expect(!internalMatchGlob("*.c", "any.cpp"));
}
