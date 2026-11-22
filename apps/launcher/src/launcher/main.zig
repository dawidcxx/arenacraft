//! Entry point: build the window layout, wire the buttons, pump messages.
//!
//! The launcher is a Windows GUI (subsystem) binary, so failures that happen
//! before the window exists are reported with a message box instead of stderr.
//!
//! The window is declared once with the `gui` layout vocabulary and replayed by
//! `gui` on every resize, so this file never deals with positions or sizes.

const std = @import("std");
const gui = @import("gui");

const game = @import("game.zig");

const allocator = std.heap.smp_allocator;

const window_title = "Arenacraft Launcher";

/// State shared with the button callbacks.
const App = struct {
    io: std.Io,
    window: *gui.Window,
};

pub fn main() void {
    // Single I/O instance for the process. `Io.Threaded` runs blocking
    // syscalls on its worker pool; `io.async`/`io.concurrent` let future work
    // (asset downloads, patching) stay off the GUI thread. Windows has no
    // `Io.Evented` backend in Zig 0.16, so Threaded is the implementation.
    var threaded: std.Io.Threaded = .init(allocator, .{});
    defer threaded.deinit();
    const io = threaded.io();

    const window = gui.createWindow(.{
        .title = window_title,
        .size = .{ .width = 560, .height = 360 },
    }) catch |err| {
        var buffer: [256]u8 = undefined;
        const text = std.fmt.bufPrint(
            &buffer,
            "Could not create the launcher window ({s}).",
            .{@errorName(err)},
        ) catch "Could not create the launcher window.";
        gui.showMessage(.critical, window_title, text);
        return;
    };
    defer window.deinit();

    // `window` outlives the callbacks: `gui.run` keeps this stack frame alive.
    var app = App{ .io = io, .window = window };

    buildUi(&app) catch {
        gui.showMessage(.critical, window_title, "Could not lay out the launcher window.");
        return;
    };

    gui.run();
}

/// Declare the window content. `gui` re-runs this tree on resize.
///
/// Layout: a padded column that fills the client area. The header row and the
/// description size themselves to their text (the description wraps when the
/// window is narrow), a growing spacer absorbs the leftover height, and the
/// action row spans the column and right-aligns its buttons.
fn buildUi(app: *App) !void {
    const window = app.window;

    try window.beginLayout();

    try window.container(.{
        .sizing = .{ .width = .{ .grow = .{} }, .height = .{ .grow = .{} } },
        .padding = .all(24),
        .gap = 16,
        .direction = .top_to_bottom,
    });

    try window.container(.{ .gap = 12, .alignment = .{ .y = .center } });
    _ = try window.addLabel(.{ .text = "Arenacraft", .font = .title });
    try window.endContainer();

    _ = try window.addLabel(.{
        .text = "World of Warcraft 3.3.5a arena server. The launcher looks for Wow.exe " ++
            "next to itself, so keep it in your game folder. Narrow the window to see " ++
            "the text re-wrap.",
        .wrap = .words,
    });

    // Spacer: takes every pixel the content does not.
    try window.container(.{ .sizing = .{ .height = .{ .grow = .{} } } });
    try window.endContainer();

    try window.container(.{
        .sizing = .{ .width = .{ .grow = .{} } },
        .gap = 10,
        .alignment = .{ .x = .right },
    });
    _ = try window.addButton(.{
        .text = "Launch Game",
        .sizing = .{ .width = .{ .fixed = 170 }, .height = .{ .fixed = 34 } },
        .onClick = .{ .function = onLaunchGame, .context = app },
    });
    _ = try window.addButton(.{
        .text = "Quit",
        .sizing = .{ .width = .{ .fixed = 90 }, .height = .{ .fixed = 34 } },
        .onClick = .{ .function = onQuit, .context = app },
    });
    try window.endContainer();

    try window.endContainer();

    try window.endLayout();
}

fn onLaunchGame(context: ?*anyopaque) void {
    const app: *App = @ptrCast(@alignCast(context orelse return));

    const outcome = game.launchWow(app.io, allocator) catch {
        gui.showMessage(.critical, window_title, "Not enough memory to start the game.");
        return;
    };

    switch (outcome) {
        .started => app.window.close(),
        .failed => |text| {
            defer allocator.free(text);
            gui.showMessage(.critical, window_title, text);
        },
    }
}

fn onQuit(context: ?*anyopaque) void {
    const app: *App = @ptrCast(@alignCast(context orelse return));
    app.window.close();
}
