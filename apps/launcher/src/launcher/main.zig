//! Entry point: startup and the window.
//!
//! The window is a Windows GUI (subsystem) binary, so failures before the
//! window exists are reported with a message box instead of stderr. Views and
//! their callbacks live in `app.zig`, `home_view.zig` and `realms_view.zig`.

const std = @import("std");
const gui = @import("gui");

const app_mod = @import("app.zig");
const install = @import("install.zig");
const realmlist = @import("realmlist.zig");

const allocator = app_mod.allocator;
const alert = app_mod.alert;

pub fn main() void {
    // Single I/O instance for the process. `Io.Threaded` runs blocking
    // syscalls on its worker pool; `io.async`/`io.concurrent` let future work
    // (asset downloads, patching) stay off the GUI thread. Windows has no
    // `Io.Evented` backend in Zig 0.16, so Threaded is the implementation.
    var threaded: std.Io.Threaded = .init(allocator, .{});
    defer threaded.deinit();
    const io = threaded.io();

    const install_dir = install.dir(io, allocator) catch |err| {
        if (err == error.OutOfMemory) unreachable;
        alert(.critical, "Could not determine where Arenacraft is installed ({s}).", .{@errorName(err)});
        return;
    };
    defer allocator.free(install_dir);

    // The installation handle stays with main; `Client` only borrows it.
    var game_dir = std.Io.Dir.cwd().openDir(io, install_dir, .{ .iterate = true }) catch |err| {
        if (err == error.OutOfMemory) unreachable;
        alert(.critical, "Could not open the game folder:\n\n{s}\n\n{s}", .{ install_dir, @errorName(err) });
        return;
    };
    defer game_dir.close(io);

    var client = realmlist.Client.init(io, allocator, game_dir) catch |err| {
        if (err == error.OutOfMemory) unreachable;
        alert(.critical, "Could not inspect the game folder for realmlist.wtf ({s}).", .{@errorName(err)});
        return;
    };
    defer client.deinit();

    var realms = client.load(io) catch |err| {
        if (err == error.OutOfMemory) unreachable;
        alert(.critical, "Could not read the realm list ({s}).", .{@errorName(err)});
        return;
    };
    defer realms.deinit();

    realms.ensurePresets() catch unreachable;

    const window = gui.createWindow(.{
        .io = io,
        .title = app_mod.title,
        .size = .{ .width = 640, .height = 480 },
    }) catch |err| {
        if (err == error.OutOfMemory) unreachable;
        alert(.critical, "Could not create the launcher window ({s}).", .{@errorName(err)});
        return;
    };
    defer window.deinit();

    // `window` outlives the callbacks: `gui.run` keeps this stack frame alive.
    var app = app_mod.App{
        .io = io,
        .window = window,
        .install_dir = install_dir,
        .client = client,
        .realms = realms,
    };

    app.showView(.home);
    gui.run();
}
