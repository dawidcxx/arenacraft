const std = @import("std");

pub const CommonBuildOptions = struct {
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    boost_path: []const u8,
};

pub const Exts = []const []const u8;
pub const JUST_CPP: Exts = &[_][]const u8{".cpp"};
pub const JUST_C: Exts = &[_][]const u8{".c"};
pub const JUST_CC: Exts = &[_][]const u8{".cc"};
pub const JUST_H: Exts = &[_][]const u8{".h"};

pub fn getAllSources(
    gpa: std.mem.Allocator,
    dir: []const u8,
    comptime extensions: Exts,
) !std.ArrayListUnmanaged([]const u8) {
    var out = try std.ArrayListUnmanaged([]const u8).initCapacity(gpa, 128);
    const target_dir = try std.fs.cwd().openDir(dir, .{ .iterate = true });
    try scanCppRecursive(gpa, target_dir, &out, extensions);
    return out;
}

pub fn filterOutFile(srcs: *std.ArrayListUnmanaged([]const u8), file_name: []const u8) void {
    for (srcs.items, 0..) |src, i| {
        if (std.mem.endsWith(u8, src, file_name)) {
            _ = srcs.swapRemove(i);
            return;
        }
    }
}

// retrieve all directories from the provided 'dir' directory
pub fn getAllFolders(gpa: std.mem.Allocator, dir: []const u8) !std.ArrayListUnmanaged([]const u8) {
    var out = try std.ArrayListUnmanaged([]const u8).initCapacity(gpa, 32);
    const target_dir = try std.fs.cwd().openDir(dir, .{ .iterate = true });
    try scanFoldersRecursive(gpa, target_dir, &out);
    return out;
}

pub fn getPrefixedCppSrcs(
    gpa: std.mem.Allocator,
    prefix: []const u8,
    srcs: []const []const u8,
) !std.ArrayListUnmanaged([]const u8) {
    var out = try std.ArrayListUnmanaged([]const u8).initCapacity(gpa, srcs.len);
    for (srcs) |src| {
        const joined = try std.mem.concat(gpa, u8, &.{ prefix, src });
        // std.log.debug("Adding to sourceset: '{s}'", .{joined});
        try out.append(gpa, joined);
    }
    return out;
}

//
// INTERNALS
//
fn scanCppRecursive(
    gpa: std.mem.Allocator,
    curr: std.fs.Dir,
    output: *std.ArrayListUnmanaged([]const u8),
    comptime extensions: Exts,
) !void {
    const root = try std.process.getCwdAlloc(gpa);
    defer gpa.free(root);

    var it = curr.iterate();
    while (try it.next()) |entry| {
        if (entry.kind == .directory) {
            const child_dir = try curr.openDir(entry.name, .{ .iterate = true });
            try scanCppRecursive(gpa, child_dir, output, extensions);
            continue;
        }

        inline for (extensions) |ext| {
            if (std.mem.endsWith(u8, entry.name, ext)) {
                const cpp_file_path = try curr.realpathAlloc(gpa, entry.name);
                defer gpa.free(cpp_file_path);
                const found_cpp_file_path = try std.fs.path.relative(gpa, root, cpp_file_path);

                // std.log.debug("Adding cpp file = '{s}' to source set", .{found_cpp_file_path});
                try output.append(gpa, found_cpp_file_path);
            }
        }
    }
}

fn scanFoldersRecursive(
    gpa: std.mem.Allocator,
    curr: std.fs.Dir,
    output: *std.ArrayListUnmanaged([]const u8),
) !void {
    const root = try std.process.getCwdAlloc(gpa);
    defer gpa.free(root);
    var it = curr.iterate();
    while (try it.next()) |entry| {
        if (entry.kind == .directory) {
            const folder_path = try curr.realpathAlloc(gpa, entry.name);
            defer gpa.free(folder_path);
            const found_folder_path = try std.fs.path.relative(gpa, root, folder_path);

            try output.append(gpa, found_folder_path);

            // Recursively scan subdirectories
            const child_dir = try curr.openDir(entry.name, .{ .iterate = true });
            try scanFoldersRecursive(gpa, child_dir, output);
        }
    }
}
