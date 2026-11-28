const std = @import("std");
const Translator = @import("translate_c").Translator;

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
var CLAY_MOD: ?*std.Build.Module = null;
var GUI_MOD: ?*std.Build.Module = null;
var LIB_TEST_STEP: *std.Build.Step = undefined;
var CLAY_TEST_STEP: *std.Build.Step = undefined;
var REALMLIST_TEST_STEP: *std.Build.Step = undefined;

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
    try addClay(b, target, optimize);
    try addGui(b, target, optimize);
    try addLauncher(b, target, optimize, console_window);
    try addRealmlistTests(b, optimize);

    const all_test = b.step("test", "Run all tests");
    all_test.dependOn(LIB_TEST_STEP);
    all_test.dependOn(CLAY_TEST_STEP);
    all_test.dependOn(REALMLIST_TEST_STEP);
}

/// Declares the shared `lib` module: the raw Win32 translation.
fn addLib(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) !void {
    const mod = b.createModule(.{
        .root_source_file = b.path("src/lib/root.zig"),
        .target = target,
        .optimize = optimize,
        // windows.h needs the MinGW-w64 headers and the C runtime Zig bundles
        // for `-windows-gnu`.
        .link_libc = true,
    });

    // `@cImport` was removed in Zig 0.17; windows.h is translated by the
    // official translate-c package instead and imported as the `c` module.
    // The translator itself runs on the host (it is a build tool), so it is
    // built for the host while emitting bindings for the windows target.
    const translate_c = b.dependency("translate_c", .{
        .target = b.resolveTargetQuery(.{}),
        .optimize = optimize,
    });
    const win32_c: Translator = .init(translate_c, .{
        .name = "win32_c",
        .c_source_file = b.addWriteFiles().add("win32_c.h",
            // Skip the kitchen sink: lean headers + no min/max macros, the two
            // defines every sane translation unit sets before including
            // windows.h. MinGW's fortified string inlines (wcscat/wcscpy)
            // translate into local declarations that Zig rejects in
            // ReleaseSafe ("unused local constant"); they are unused here and
            // Zig has its own safety checks, so fortify stays off in every
            // optimize mode.
            \\#define WIN32_LEAN_AND_MEAN 1
            \\#define NOMINMAX 1
            \\#define _FORTIFY_SOURCE 0
            \\#include <windows.h>
            \\
        ),
        .target = target,
        .optimize = optimize,
    });
    mod.addImport("c", win32_c.mod);
    LIB_MOD = mod;

    const tst = b.addTest(.{ .root_module = mod });
    const run = b.addRunArtifact(tst);
    // forwards `zig build <step> -- <args>` at make time (zig 0.17)
    run.addPassthruArgs();
    LIB_TEST_STEP = b.step("lib-test", "Run lib tests");
    LIB_TEST_STEP.dependOn(&run.step);
}

/// Declares the `clay` module: the ported layout engine.
fn addClay(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) !void {
    const mod = b.createModule(.{
        .root_source_file = b.path("src/clay/root.zig"),
        .target = target,
        .optimize = optimize,
    });
    CLAY_MOD = mod;

    // Layout is pure computation with no libc or Win32 dependency, so the tests
    // build for the host and run directly instead of through wine.
    const host_mod = b.createModule(.{
        .root_source_file = b.path("src/clay/root.zig"),
        .target = b.resolveTargetQuery(.{}),
        .optimize = optimize,
    });
    const tst = b.addTest(.{ .root_module = host_mod });
    const run = b.addRunArtifact(tst);
    // forwards `zig build <step> -- <args>` at make time (zig 0.17)
    run.addPassthruArgs();
    CLAY_TEST_STEP = b.step("clay-test", "Run the clay layout tests (native)");
    CLAY_TEST_STEP.dependOn(&run.step);
}

/// Realmlist parsing and file selection are plain `std`, so their tests build
/// for the host and run natively instead of through wine.
fn addRealmlistTests(
    b: *std.Build,
    optimize: std.builtin.OptimizeMode,
) !void {
    const mod = b.createModule(.{
        .root_source_file = b.path("src/launcher/realmlist.zig"),
        .target = b.resolveTargetQuery(.{}),
        .optimize = optimize,
    });

    const tst = b.addTest(.{ .root_module = mod });
    const run = b.addRunArtifact(tst);
    // forwards `zig build <step> -- <args>` at make time (zig 0.17)
    run.addPassthruArgs();
    REALMLIST_TEST_STEP = b.step("realmlist-test", "Run the realmlist tests (native)");
    REALMLIST_TEST_STEP.dependOn(&run.step);
}

/// Declares the `gui` module: the launcher's scoped Win32 GUI layer.
fn addGui(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) !void {
    const lib_mod = LIB_MOD orelse return error.MissingModule;
    const clay_mod = CLAY_MOD orelse return error.MissingModule;

    const mod = b.createModule(.{
        .root_source_file = b.path("src/gui/root.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .imports = &.{
            .{ .name = "lib", .module = lib_mod },
            .{ .name = "clay", .module = clay_mod },
        },
    });
    // Fonts and brushes are GDI; Zig's windows-gnu link line does not include
    // gdi32 by default (unlike user32/kernel32).
    mod.linkSystemLibrary("gdi32", .{ .use_pkg_config = .no });
    GUI_MOD = mod;
}

/// Declares the `launcher` executable on top of the `lib` and `gui` modules.
fn addLauncher(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    console_window: bool,
) !void {
    const lib_mod = LIB_MOD orelse return error.MissingModule;
    const gui_mod = GUI_MOD orelse return error.MissingModule;

    const mod = b.createModule(.{
        .root_source_file = b.path("src/launcher/main.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .imports = &.{
            .{ .name = "lib", .module = lib_mod },
            .{ .name = "gui", .module = gui_mod },
        },
    });
    // The application icon lives in the PE resource table (launcher.rc), so
    // Explorer and shortcuts show it too, not just the window.
    mod.addWin32ResourceFile(.{ .file = b.path("launcher.rc") });

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
    // forwards `zig build <step> -- <args>` at make time (zig 0.17)
    run.addPassthruArgs();
    // The artifact is a foreign (PE) binary for a non-Windows host: let Zig
    // deal with it. `zig build run-launcher -fwine` runs it through wine
    // (found on PATH, WINEDEBUG silenced), otherwise a host binfmt handler can
    // execute it directly. Nothing wine-specific is wired into the build graph.
    const run_step = b.step("run-launcher", "Run launcher.exe (foreign binary: -fwine or host binfmt)");
    run_step.dependOn(&run.step);
}
