const std = @import("std");
const utils = @import("./build_utils.zig");
const deps = @import("./deps.zig");
const common = @import("./common.zig");

const CommonBuildOptions = utils.CommonBuildOptions;

var database_lib: *std.Build.Step.Compile = undefined;

pub fn buildDatabase(b: *std.Build, opts: CommonBuildOptions) !void {
    try buildDatabaseLib(b, opts);
}

//
// Links
//
pub fn linkDatabase(b: *std.Build, target: *std.Build.Module) void {
    target.linkLibrary(database_lib);
    for (database_lib.root_module.include_dirs.items) |incl| {
        switch (incl) {
            .path => |p| {
                target.addIncludePath(p.dupe(b));
            },
            else => {},
        }
    }
}

//
// Builds
//
fn buildDatabaseLib(b: *std.Build, options: CommonBuildOptions) !void {
    const mod = b.createModule(.{
        .target = options.target,
        .optimize = options.optimize,
        .link_libcpp = true,
    });

    deps.linkFmt(b, mod);
    common.linkCommon(b, mod);

    // Link MySQL library
    mod.linkSystemLibrary("mysqlclient", .{});

    mod.addIncludePath(b.path("./src/server/database"));
    const subdirs = try utils.getAllFolders(b.allocator, "./src/server/database");
    for (subdirs.items) |dir| {
        // std.log.info("Appending to database include path '{s}'", .{dir});
        mod.addIncludePath(b.path(dir));
    }

    var srcs = try utils.getAllSources(b.allocator, "./src/server/database", utils.JUST_CPP);

    // Skip problematic files that have C++20 consteval issues with Zig
    utils.filterOutFile(&srcs, "DatabaseLoader.cpp");
    utils.filterOutFile(&srcs, "DatabaseWorkerPool.cpp");
    utils.filterOutFile(&srcs, "DBUpdater.cpp");
    utils.filterOutFile(&srcs, "UpdateFetcher.cpp");
    // Filter files with heavy logging that cause consteval issues
    utils.filterOutFile(&srcs, "Field.cpp");

    for (srcs.items) |src| {
        // std.log.info("Adding database source '{s}'", .{src});
        mod.addCSourceFile(.{
            .file = b.path(src),
            .flags = &.{
                "-std=c++23",
                "-Wno-error",
            },
        });
    }

    database_lib = b.addLibrary(.{
        .name = "database",
        .root_module = mod,
    });

    b.installArtifact(database_lib);
}
