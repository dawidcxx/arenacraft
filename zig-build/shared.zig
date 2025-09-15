const std = @import("std");
const utils = @import("./build_utils.zig");

const CommonBuildOptions = utils.CommonBuildOptions;

var shared_lib: *std.Build.Step.Compile = undefined;

pub fn buildShared(b: *std.Build, opts: CommonBuildOptions) !void {
    try buildShared(b, opts);
}

//
// Links
//
pub fn linkShared(b: *std.Build, target: *std.Build.Module) void {
    target.linkLibrary(shared_lib);
    for (shared_lib.root_module.include_dirs.items) |incl| {
        target.addIncludePath(incl.path.dupe(b));
    }
}

//
// Builds
//

fn buildSharedLib(b: *std.Build, options: CommonBuildOptions) !void {
    const mod = b.createModule(.{
        .target = options.target,
        .optimize = options.optimize,
        .link_libcpp = true,
    });

    mod.addIncludePath(b.path("./src/server/shared"));
    const subdirs = try utils.getAllFolders(b.allocator, "./src/server/shared");
    for (subdirs.items) |dir| {
        mod.addIncludePath(b.path(dir));
    }

    const srcs = try utils.getPrefixedCppSrcs(b.allocator, "./server/shared", utils.JUST_CPP);
    for (srcs.items) |src| {
        mod.addCSourceFile(.{
            .file = b.path(src),
            .flags = &.{"-std=c++20"},
        });
    }

    shared_lib = b.addLibrary(.{
        .name = "shared",
        .root_module = mod,
    });
}
