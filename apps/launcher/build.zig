const std = @import("std");

/// The launcher is a Windows client by definition, so every module and build
/// step below assumes `x86_64-windows-gnu`. There is deliberately no
/// `-Dtarget` knob: execution is the host's business (native on Windows,
/// `binfmt_misc` or `zig build -fwine` elsewhere).
const windows_target_query: std.Target.Query = .{
    .cpu_arch = .x86_64,
    .os_tag = .windows,
    .abi = .gnu,
};

var LIB_MOD: ?*std.Build.Module = null;
var LIB_TEST_STEP: *std.Build.Step = undefined;

pub fn build(b: *std.Build) !void {
    const target = b.resolveTargetQuery(windows_target_query);
    const optimize = b.standardOptimizeOption(.{});

    // GUI subsystem by default: a launcher should not flash a console at
    // players. The console is a debugging aid, opted into per invocation
    // (`-Dconsole-window=true`), never implied by the host or build mode.
    const console_window = b.option(
        bool,
        "console-window",
        "Build with the console subsystem (debug aid; default: off)",
    ) orelse false;

    try addLib(b, target, optimize);
    try addLauncher(b, target, optimize, console_window);

    const all_test = b.step("test", "Run all tests");
    all_test.dependOn(LIB_TEST_STEP);
}

/// Declares the shared `lib` module: Win32 bindings and launcher logic.
fn addLib(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) !void {
    const mod = b.createModule(.{
        .root_source_file = b.path("src/lib/root.zig"),
        .target = target,
        .optimize = optimize,
        // win32.zig @cImports windows.h: that needs the MinGW-w64 headers and
        // the C runtime Zig bundles for `-windows-gnu`.
        .link_libc = true,
    });
    LIB_MOD = mod;

    const tst = b.addTest(.{ .root_module = mod });
    const run = b.addRunArtifact(tst);
    if (b.args) |args| {
        run.addArgs(args);
    }
    LIB_TEST_STEP = b.step("lib-test", "Run lib tests");
    LIB_TEST_STEP.dependOn(&run.step);
}

/// Declares the `launcher` executable on top of the `lib` module.
fn addLauncher(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    console_window: bool,
) !void {
    const lib_mod = LIB_MOD orelse return error.MissingModule;

    const mod = b.createModule(.{
        .root_source_file = b.path("src/launcher/main.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .imports = &.{
            .{ .name = "lib", .module = lib_mod },
        },
    });
    const exe = b.addExecutable(.{
        .name = "launcher",
        .root_module = mod,
    });
    exe.subsystem = if (console_window) .console else .windows;

    const install = b.addInstallArtifact(exe, .{});
    b.getInstallStep().dependOn(&install.step); // plain `zig build`
    const build_step = b.step("launcher", "Build launcher.exe");
    build_step.dependOn(&install.step);

    const run = b.addRunArtifact(exe);
    run.step.dependOn(&install.step);
    if (b.args) |args| {
        run.addArgs(args);
    }
    // The artifact is a foreign (PE) binary for a non-Windows host: let Zig
    // deal with it. `zig build run-launcher -fwine` runs it through wine
    // (found on PATH, WINEDEBUG silenced), otherwise a host binfmt handler can
    // execute it directly. Nothing wine-specific is wired into the build graph.
    const run_step = b.step("run-launcher", "Run launcher.exe (foreign binary: -fwine or host binfmt)");
    run_step.dependOn(&run.step);
}
