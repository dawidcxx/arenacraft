const std = @import("std");
const utils = @import("./zig-build/build_utils.zig");
const deps = @import("./zig-build/deps.zig");
const common = @import("./zig-build/common.zig");

// src
var common_mod_lib: *std.Build.Step.Compile = undefined;
var database_mod_lib: *std.Build.Step.Compile = undefined;
var shared_mod_lib: *std.Build.Step.Compile = undefined;

pub fn build(b: *std.Build) void {
    const env_map = std.process.getEnvMap(b.allocator) catch {
        @panic("Failed to read env");
    };

    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const boost_path = env_map.get("BOOST_PATH") orelse {
        @panic("no boost path");
    };

    const options: utils.CommonBuildOptions = .{
        .target = target,
        .optimize = optimize,
        .boost_path = boost_path,
    };

    deps.buildDeps(b, options) catch {
        @panic("FAIL: buildDeps");
    };

    common.buildCommon(b, options) catch {
        @panic("FAIL: buildCommon");
    };

    // buildDeps(b, options) catch {
    //     @panic("Failed to buildDeps");
    // };
    //
    // buildCommonLibrary(b, options) catch {
    //     @panic("Failed to buildCommonLibrary");
    // };
    //
    // buildDatabaseLibrary(b, options) catch {
    //     @panic("Failed to buildDatabaseLibrary");
    // };
}

// fn buildCommonLibrary(b: *std.Build, opts: CommonBuildOptions) !void {
//     const gpa = b.allocator;
//     const target = opts.target;
//     const optimize = opts.optimize;
//
//     const common_mod = b.createModule(.{
//         .target = target,
//         .optimize = optimize,
//         .link_libcpp = true,./deps/g3dlite/source/uint128.cpp
// info: Adding: ./deps/g3dlite/source/UprightFrame.cpp
// //     });
//
//     linkG3DLiteLib(b, common_mod);
//     linkDetour(b, common_mod);
//     linkFmt(b, common_mod);
//
//     common_mod.addIncludePath(b.path("./src/common"));
//
//     const common_subdirs = try getAllFolders(gpa, "./src/common");
//     for (common_subdirs.items) |dir| {
//         common_mod.addIncludePath(b.path(dir));
//     }
//
//     const common_srcs = try getAllSources(gpa, "./src/common", JUST_CPP);
//     for (common_srcs.items) |src| {
//         common_mod.addCSourceFile(.{
//             .file = b.path(src),
//             .flags = &.{
//                 "-std=c++20",
//             },
//         });
//     }
//
//     common_mod_lib = b.addLibrary(.{
//         .name = "common",
//         .linkage = .static,
//         .root_module = common_mod,
//     });
//
//     b.installArtifact(common_mod_lib);
// }
//
// fn linkCommon(b: *std.Build, target: *std.Build.Module) void {
//     target.linkLibrary(common_mod_lib);
//     for (common_mod_lib.root_module.include_dirs.items) |incl| {
//         switch (incl) {
//             .path => |p| {
//                 // std.log.info("Linking {s}", .{p.getDisplayName()});
//                 // TODO: filter out inner dependenecy include paths
//                 target.addIncludePath(p.dupe(b));
//             },
//             else => {},
//         }
//     }
// }
//
// fn buildDatabaseLibrary(b: *std.Build, opts: CommonBuildOptions) !void {
//     const gpa = b.allocator;
//     const target = opts.target;
//     const optimize = opts.optimize;
//
//     const database_mod = b.createModule(.{
//         .target = target,
//         .optimize = optimize,
//         .link_libcpp = true,
//     });
//
//     database_mod.addIncludePath(b.path("./src/server/database"));
//
//     const database_subdirs = try getAllFolders(gpa, "./src/server/database");
//     for (database_subdirs.items) |dir| {
//         database_mod.addIncludePath(b.path(dir));
//     }
//
//     linkCommon(b, database_mod);
//
//     // Link MySQL library
//     database_mod.linkSystemLibrary("mysqlclient", .{});
//
//     const database_srcs = try getAllSources(gpa, "./src/server/database", JUST_CPP);
//     for (database_srcs.items) |src| {
//         // Skip problematic files that have C++20 consteval issues with Zig
//         if (std.mem.endsWith(u8, src, "DatabaseLoader.cpp")) {
//             continue;
//         }
//
//         database_mod.addCSourceFile(.{
//             .file = b.path(src),
//             .flags = &.{
//                 "-std=c++20",
//                 "-Wno-error",./deps/g3dlite/source/uint128.cpp
// info: Adding: ./deps/g3dlite/source/UprightFrame.cpp
//             },
//         });
//     }
//
//     database_mod_lib = b.addLibrary(.{
//         .name = "database",
//         .linkage = .static,
//         .root_module = database_mod,
//     });
//
//     b.installArtifact(database_mod_lib);
// }
//
// fn buildSharedLibrary(b: *std.Build, opts: CommonBuildOptions) !void {
//     const gpa = b.allocator;
//     const target = opts.target;
//     const optimize = opts.optimize;
//
//     const shared_mod = b.createModule(.{
//         .target = target,
//         .optimize = optimize,
//         .link_libcpp = true,
//     });
//
//     shared_mod.addIncludePath(b.path("./src/server/shared"));
//
//     const shared_subdirs = try getAllFolders(gpa, "./src/server/shared");
//     for (shared_subdirs.items) |dir| {
//         shared_mod.addIncludePath(b.path(dir));
//     }
//
//     linkCommon(b, shared_mod);
//     linkDatabase(b, shared_mod);
//
//     const shared_srcs = try getAllSources(gpa, "./src/server/shared", JUST_CPP);
//     for (shared_srcs.items) |src| {
//         shared_mod.addCSourceFile(.{
//             .file = b.path(src),
//             .flags = &.{
//                 "-std=c++20",
//                 "-fno-immediate-escalation",
//             },
//         });
//     }
//
//     shared_mod_lib = b.addLibrary(.{
//         .name = "shared",
//         .linkage = .static,
//         .root_module = shared_mod,
//     });
//
//     b.installArtifact(shared_mod_lib);
// }
//
// fn linkShared(b: *std.Build, target: *std.Build.Module) void {
//     target.linkLibrary(shared_mod_lib);
//     for (shared_mod_lib.root_module.include_dirs.items) |incl| {
//         switch (incl) {
//             .path => |p| {
//                 target.addIncludePath(p.dupe(b));
//             },
//             else => {},
//         }
//     }
// }
//
// fn buildAuthServer(b: *std.Build, opts: CommonBuildOptions) !void {
//     const gpa = b.allocator;
//     const target = opts.target;
//     const optimize = opts.optimize;
//
//     const authserver_mod = b.createModule(.{
//         .target = target,
//         .optimize = optimize,
//         .link_libcpp = true,
//     });
//
//     authserver_mod.addIncludePath(b.path("./src/server/apps/authserver"));
//     authserver_mod.addIncludePath(b.path("./src/server/apps/authserver/Authentication"));
//     authserver_mod.addIncludePath(b.path("./src/server/apps/authserver/Server"));
//
//     // Add shared and database include paths
//     authserver_mod.addIncludePath(b.path("./src/server/shared"));
//     authserver_mod.addIncludePath(b.path("./src/server/database"));
//     authserver_mod.addIncludePath(b.path("./src/server/database/Database"));
//
//     linkCommon(b, authserver_mod);
//
//     const authserver_cpp_srcs = try getAllSources(gpa, "./src/server/apps/authserver", JUST_CPP);
//     for (authserver_cpp_srcs.items) |src| {
//         authserver_mod.addCSourceFile(.{
//             .file = b.path(src),
//             .flags = &.{
//                 "-std=c++20",
//             },
//         });
//     }
//
//     const authserver_config_header = std.Build.Step.ConfigHeader.create(b, .{});
//     authserver_mod.addConfigHeader(authserver_config_header);
//
//     const authserver_exe = b.addExecutable(.{
//         .name = "authserver",
//         .root_module = authserver_mod,
//     });
//
//     b.installArtifact(authserver_exe);
// }
//
