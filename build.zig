const std = @import("std");
const utils = @import("./zig-build/build_utils.zig");
const deps = @import("./zig-build/deps.zig");
const common = @import("./zig-build/common.zig");
const database = @import("./zig-build/database.zig");
const shared = @import("./zig-build/shared.zig");
const zcc = @import("compile_commands");

var targets: std.ArrayList(*std.Build.Step.Compile) = undefined;
var run_minimal: *std.Build.Step.Run = undefined;

pub fn build(b: *std.Build) void {
    targets = .init(b.allocator);

    const env_map = std.process.getEnvMap(b.allocator) catch {
        @panic("Failed to read env");
    };

    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const boost_incl_path = env_map.get("BOOST_PATH") orelse {
        @panic("no boost path");
    };

    const options: utils.CommonBuildOptions = .{
        .target = target,
        .optimize = optimize,
        .boost_path = boost_incl_path,
    };

    deps.buildDeps(b, options) catch {
        @panic("FAIL: buildDeps");
    };

    common.buildCommon(b, options) catch {
        @panic("FAIL: buildCommon");
    };

    database.buildDatabase(b, options) catch {
        @panic("FAIL: buildDatabase");
    };

    shared.buildShared(b, options) catch {
        @panic("FAIL: buildShared");
    };

    buildAuthServer(b, options) catch {
        @panic("FAIL: failed to build authServer");
    };

    buildMinimal(b, options) catch {
        @panic("FAIL: failed to build buildMinimal");
    };

    const cdb_step = zcc.createStep(b, "cdb", targets.toOwnedSlice() catch unreachable);
    cdb_step.dependOn(&run_minimal.step);
}

fn buildMinimal(b: *std.Build, opts: utils.CommonBuildOptions) !void {
    const target = opts.target;
    const optimize = opts.optimize;

    const mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libcpp = null,
    });

    mod.addCSourceFile(.{
        .file = b.path("./zig-build/Minimal.cpp"),
        .flags = &.{
            "-std=c++23",
            "-stdlib=libc++",
        },
    });

    deps.linkBoost(b, mod);
    deps.linkFmt(b, mod);

    const exe = b.addExecutable(.{
        .name = "minimal",
        .root_module = mod,
    });

    try targets.append(exe);
    b.installArtifact(exe);

    run_minimal = b.addRunArtifact(exe);
    const run = b.step("run-minimal", "runs Minimal.cpp - checks if linkeage is OK-ish");
    run.dependOn(&run_minimal.step);
}

fn buildAuthServer(b: *std.Build, opts: utils.CommonBuildOptions) !void {
    const gpa = b.allocator;
    const target = opts.target;
    const optimize = opts.optimize;

    const authserver_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libcpp = true,
    });

    deps.linkBoost(b, authserver_mod);
    common.linkCommon(b, authserver_mod);
    shared.linkShared(b, authserver_mod);
    database.linkDatabase(b, authserver_mod);

    authserver_mod.addIncludePath(b.path("./src/server/apps/authserver"));
    authserver_mod.addIncludePath(b.path("./src/server/apps/authserver/Authentication"));
    authserver_mod.addIncludePath(b.path("./src/server/apps/authserver/Server"));

    const authserver_cpp_srcs = try utils.getAllSources(gpa, "./src/server/apps/authserver", utils.JUST_CPP);
    for (authserver_cpp_srcs.items) |src| {
        authserver_mod.addCSourceFile(.{
            .file = b.path(src),
            .flags = &.{
                "-std=c++23",
            },
        });
    }

    const authserver_config_header = std.Build.Step.ConfigHeader.create(b, .{});
    authserver_mod.addConfigHeader(authserver_config_header);

    const authserver_exe = b.addExecutable(.{
        .name = "authserver",
        .root_module = authserver_mod,
    });

    try targets.append(authserver_exe);

    b.installArtifact(authserver_exe);
}
