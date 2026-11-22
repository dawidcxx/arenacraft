//! Entry point: create the window, add the launch button, pump messages.
//!
//! The launcher is a Windows GUI (subsystem) binary, so failures that happen
//! before the window exists are reported with a message box instead of stderr.

const std = @import("std");
const gui = @import("gui");

const game = @import("game.zig");

const allocator = std.heap.smp_allocator;

const window_title = "Arenacraft Launcher";

/// State shared with the button callback.
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
        .size = .{ .width = 520, .height = 280 },
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

    // `window` outlives the callback: `gui.run` keeps this stack frame alive.
    var app = App{ .io = io, .window = window };

    _ = window.addButton(.{
        .text = "Launch Game",
        .onClick = .{ .function = onLaunchGame, .context = &app },
    }) catch {
        gui.showMessage(.critical, window_title, "Could not create the Launch Game button.");
        return;
    };

    gui.run();
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
