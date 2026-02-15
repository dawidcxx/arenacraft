const std = @import("std");
// Import the local cppkit-zig build helper directly so edits in the workspace
// are used immediately instead of relying on Zig's package cache.
const cpp = @import("./cppkit-zig/build.zig");

var INCLUDE_PATH: []const u8 = undefined;

pub fn build(b: *std.Build) void {
    var env_map = std.process.getEnvMap(b.allocator) catch @panic("Failed to get environment variables");
    defer env_map.deinit();
    INCLUDE_PATH = env_map.get("FLAKE_INCLUDES") orelse @panic("missing FLAKE_INCLUDES");

    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    buildAuth(b, target, optimize);

    cpp.addCompileCommandsStep(b);
}

fn buildAuth(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) void {
    const auth_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libcpp = true,
    });

    const auth_srcs = cpp.querySources(b.allocator, "./src/auth/", .{
        .extensions = cpp.Exts.JUST_CPP,
        .recursive = true,
    });

    auth_module.addCSourceFiles(.{
        .files = auth_srcs.get(),
        .language = .cpp,
        .flags = &.{
            "-std=c++20",
        },
    });

    auth_module.addIncludePath(b.path("./src/auth/Include/"));

    addSystemLibIncludes(auth_module);
    auth_module.linkSystemLibrary("openssl", .{});
    auth_module.linkSystemLibrary("mysqlclient", .{});

    const auth_app_exe = b.addExecutable(.{
        .name = "auth_app",
        .root_module = auth_module,
    });

    cpp.addCompileCommands(auth_app_exe);
    addBoost(b, target, optimize, auth_app_exe);

    const app_exe_run = b.addRunArtifact(auth_app_exe);
    if (b.args) |args| {
        app_exe_run.addArgs(args);
    }

    const run_step = b.step("run-auth", "Run the Main application");
    run_step.dependOn(&app_exe_run.step);

    const output_exe = b.addInstallArtifact(auth_app_exe, .{});

    b.default_step.dependOn(&output_exe.step);
}

fn addBoost(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    compile_task: *std.Build.Step.Compile,
) void {
    const boost_dep = b.dependency("boost", .{
        .target = target,
        .optimize = optimize,
        .cobalt = true,
    });
    const boost_artifact = boost_dep.artifact("boost");
    for (boost_artifact.root_module.include_dirs.items) |include_dir| {
        compile_task.root_module.include_dirs.append(b.allocator, include_dir) catch @panic("Failed to add boost include directory");
    }
    compile_task.linkLibrary(boost_artifact);
}

fn addSystemLibIncludes(mod: *std.Build.Module) void {
    var split = std.mem.splitScalar(u8, INCLUDE_PATH, ':');
    while (split.next()) |item| {
        mod.addIncludePath(.{ .cwd_relative = item });
    }
}
