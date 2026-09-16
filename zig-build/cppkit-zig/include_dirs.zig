// Flat-name include directory helpers.
//
// Common and the tools rely on every subdirectory being on the include path
// ("MapDefines.h" included flat from MapTree.cpp), mirroring CMake's
// CollectIncludeDirectories behavior.

const std = @import("std");
const Build = std.Build;

/// Add `dir` and every subdirectory of it as include paths.
pub fn addFlatIncludes(b: *Build, dir_path: []const u8, module: *Build.Module) !void {
    var dirs: std.ArrayListUnmanaged([]const u8) = .empty;
    try collectDirsRecursive(b.allocator, b.graph.io, dir_path, &dirs);

    // deterministic include order, shallow dirs first - this also makes
    // same-named headers resolve like the CMake build did (e.g.
    // map_extractor/loadlib/loadlib.h before vmap4_extractor/loadlib/loadlib.h)
    std.mem.sort([]const u8, dirs.items, {}, struct {
        fn lessThan(_: void, lhs: []const u8, rhs: []const u8) bool {
            return std.mem.lessThan(u8, lhs, rhs);
        }
    }.lessThan);

    module.addIncludePath(b.path(dir_path));
    for (dirs.items) |d| {
        module.addIncludePath(b.path(d));
    }
}

fn collectDirsRecursive(
    gpa: std.mem.Allocator,
    io: std.Io,
    dir_path: []const u8,
    out: *std.ArrayListUnmanaged([]const u8),
) !void {
    const dir = std.Io.Dir.cwd().openDir(io, dir_path, .{ .iterate = true }) catch |e| {
        std.debug.panic("Failed to openDir() '{s}', reason='{}'", .{ dir_path, e });
    };
    defer dir.close(io);

    var it = dir.iterate();
    while (try it.next(io)) |entry| {
        if (entry.kind != .directory) continue;
        const child = try std.fs.path.join(gpa, &.{ dir_path, entry.name });
        try out.append(gpa, child);
        try collectDirsRecursive(gpa, io, child, out);
    }
}
