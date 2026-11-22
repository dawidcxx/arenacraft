//! Entry point: build the window layout, wire the buttons, pump messages.
//!
//! The launcher is a Windows GUI (subsystem) binary, so failures that happen
//! before the window exists are reported with a message box instead of stderr.
//!
//! The window is declared once with the `gui` layout vocabulary and replayed by
//! `gui` on every resize, so this file never deals with positions or sizes.
//!
//! The realm picker is the launcher's one piece of game state: the remembered
//! realms are a plain list, the selected address is a text field, and the
//! chosen host is written to the client's `realmlist.wtf` when a row is picked
//! and again right before `Wow.exe` starts.

const std = @import("std");
const gui = @import("gui");

const game = @import("game.zig");
const install = @import("install.zig");
const realmlist = @import("realmlist.zig");

const allocator = std.heap.smp_allocator;

const window_title = "Arenacraft Launcher";

/// State shared with the widget callbacks. `gui.run` keeps the stack frame in
/// `main` alive, so pointers into this struct stay valid until the window is
/// destroyed.
const App = struct {
    io: std.Io,
    window: *gui.Window,
    /// Game directory, owned by `main`.
    install_dir: []const u8,
    client: realmlist.Client,
    realms: realmlist.List,
    list: *gui.ListBox,
    edit: *gui.Edit,
    realm_name: *gui.Label,
    status: *gui.Label,
};

pub fn main() void {
    // Single I/O instance for the process. `Io.Threaded` runs blocking
    // syscalls on its worker pool; `io.async`/`io.concurrent` let future work
    // (asset downloads, patching) stay off the GUI thread. Windows has no
    // `Io.Evented` backend in Zig 0.16, so Threaded is the implementation.
    var threaded: std.Io.Threaded = .init(allocator, .{});
    defer threaded.deinit();
    const io = threaded.io();

    const install_dir = install.dir(io, allocator) catch |err| {
        var buffer: [256]u8 = undefined;
        const text = std.fmt.bufPrint(
            &buffer,
            "Could not determine where Arenacraft is installed ({s}).",
            .{@errorName(err)},
        ) catch "Could not determine where Arenacraft is installed.";
        gui.showMessage(.critical, window_title, text);
        return;
    };
    defer allocator.free(install_dir);

    // The installation handle stays with main; `Client` only borrows it.
    var game_dir = std.Io.Dir.cwd().openDir(io, install_dir, .{ .iterate = true }) catch |err| {
        var buffer: [512]u8 = undefined;
        const text = std.fmt.bufPrint(
            &buffer,
            "Could not open the game folder:\n\n{s}\n\n{s}",
            .{ install_dir, @errorName(err) },
        ) catch "Could not open the game folder.";
        gui.showMessage(.critical, window_title, text);
        return;
    };
    defer game_dir.close(io);

    var client = realmlist.Client.init(io, allocator, game_dir) catch |err| {
        var buffer: [256]u8 = undefined;
        const text = std.fmt.bufPrint(
            &buffer,
            "Could not inspect the game folder for realmlist.wtf ({s}).",
            .{@errorName(err)},
        ) catch "Could not inspect the game folder.";
        gui.showMessage(.critical, window_title, text);
        return;
    };
    defer client.deinit();

    var realms = client.load(io) catch |err| {
        var buffer: [256]u8 = undefined;
        const text = std.fmt.bufPrint(
            &buffer,
            "Could not read the realm list ({s}).",
            .{@errorName(err)},
        ) catch "Could not read the realm list.";
        gui.showMessage(.critical, window_title, text);
        return;
    };
    defer realms.deinit();

    realms.ensurePresets() catch {
        gui.showMessage(.critical, window_title, "Not enough memory to read the realm list.");
        return;
    };

    const window = gui.createWindow(.{
        .title = window_title,
        .size = .{ .width = 620, .height = 420 },
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
    var app = App{
        .io = io,
        .window = window,
        .install_dir = install_dir,
        .client = client,
        .realms = realms,
        .list = undefined,
        .edit = undefined,
        .realm_name = undefined,
        .status = undefined,
    };

    buildUi(&app) catch {
        gui.showMessage(.critical, window_title, "Could not lay out the launcher window.");
        return;
    };

    gui.run();
}

/// Declare the window content. `gui` re-runs this tree on resize.
///
/// Layout: a padded column. The title and description size themselves to their
/// text, the two panes share the remaining height (remembered realms with a
/// Remove button on the left, address field and hints on the right), the status
/// line names the file being written, a growing spacer absorbs the leftover, and
/// the action row right-aligns its buttons.
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
        .text = "World of Warcraft 3.3.5a arena server. Pick a realm, then launch the game. " ++
            "The launcher rewrites the client's realmlist.wtf and keeps every realm you have " ++
            "used in the list.",
        .wrap = .words,
    });

    try window.container(.{
        .sizing = .{ .width = .{ .grow = .{} }, .height = .{ .grow = .{} } },
        .gap = 16,
    });

    // Left pane: every realm the launcher knows about.
    try window.container(.{
        .sizing = .{ .width = .{ .fixed = 240 }, .height = .{ .grow = .{} } },
        .gap = 6,
        .direction = .top_to_bottom,
    });
    _ = try window.addLabel(.{ .text = "Realms" });
    app.list = try window.addListBox(.{
        .items = app.realms.hosts.items,
        .selected = app.realms.active,
        .onSelect = .{ .function = onRealmSelect, .context = app },
        .sizing = .{
            .width = .{ .grow = .{} },
            .height = .{ .grow = .{} },
        },
    });
    _ = try window.addButton(.{
        .text = "Remove",
        .sizing = .{ .width = .{ .fixed = 90 }, .height = .{ .fixed = 28 } },
        .onClick = .{ .function = onRealmRemove, .context = app },
    });
    try window.endContainer();

    // Right pane: the address that will be written, editable.
    try window.container(.{
        .sizing = .{ .width = .{ .grow = .{} }, .height = .{ .grow = .{} } },
        .gap = 6,
        .direction = .top_to_bottom,
    });
    _ = try window.addLabel(.{ .text = "Realm address" });
    app.edit = try window.addEdit(.{
        .text = app.realms.activeHost(),
        .onEnter = .{ .function = onRealmCommit, .context = app },
        .sizing = .{
            .width = .{ .grow = .{} },
            .height = .{ .fixed = 26 },
        },
    });
    app.realm_name = try window.addLabel(.{ .text = realmlist.displayName(app.realms.activeHost()) });
    _ = try window.addLabel(.{
        .text = "Pick a realm on the left, or type a logon host such as " ++
            "logon.arenacraft.fun and press Enter.",
        .wrap = .words,
    });
    try window.endContainer();

    try window.endContainer();

    var status_buffer: [512]u8 = undefined;
    const status_text = std.fmt.bufPrint(
        &status_buffer,
        "Writes {s}",
        .{app.client.primary()},
    ) catch "Writes realmlist.wtf";
    app.status = try window.addLabel(.{ .text = status_text, .wrap = .none });

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

/// The player picked a realm from the list: put it in the address field and
/// apply it immediately.
fn onRealmSelect(context: ?*anyopaque) void {
    const app: *App = @ptrCast(@alignCast(context orelse return));
    const index = app.window.listSelection(app.list) orelse return;
    if (index >= app.realms.hosts.items.len) return;

    app.window.setEditText(app.edit, app.realms.hosts.items[index]) catch return;
    _ = applyRealm(app);
}

/// The player pressed Enter in the address field.
fn onRealmCommit(context: ?*anyopaque) void {
    const app: *App = @ptrCast(@alignCast(context orelse return));
    _ = applyRealm(app);
}

fn onLaunchGame(context: ?*anyopaque) void {
    const app: *App = @ptrCast(@alignCast(context orelse return));

    // A realm typed and not yet committed still has to reach realmlist.wtf
    // before the client reads it.
    if (!applyRealm(app)) return;

    const outcome = game.launchWow(allocator, app.install_dir) catch {
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

/// Read the realm field, remember it, write `realmlist.wtf`, and refresh the
/// UI. Returns false (after reporting why) when the realm was not applied.
fn applyRealm(app: *App) bool {
    const previous_len = app.realms.hosts.items.len;

    const text = app.window.editText(app.edit) catch {
        gui.showMessage(.critical, window_title, "Not enough memory to read the address field.");
        return false;
    };
    defer allocator.free(text);

    const host = realmlist.validate(text) orelse {
        gui.showMessage(.warning, window_title, "That is not a realm address.\n\n" ++
            "Pick a realm from the list or type a logon host such as " ++
            "logon.arenacraft.fun (letters, digits, dots and dashes only).");
        return false;
    };

    _ = app.realms.setActive(host) catch {
        gui.showMessage(.critical, window_title, "Not enough memory to remember the realm.");
        return false;
    };
    const added_host = app.realms.hosts.items.len != previous_len;

    if (!writeRealms(app)) return false;
    // Only a newly typed host changes the rows; rebuilding them from inside the
    // list's selection notification would lose the highlight.
    refreshRealmUi(app, if (added_host) .rows else .selection);

    setStatus(app, "Saved {d} realm{s} to {s}", .{
        app.realms.hosts.items.len,
        if (app.realms.hosts.items.len == 1) "" else "s",
        app.client.primary(),
    });
    return true;
}

/// The player pressed Remove: forget the highlighted realm. If it was the
/// active one, the next remembered realm becomes active and is written.
fn onRealmRemove(context: ?*anyopaque) void {
    const app: *App = @ptrCast(@alignCast(context orelse return));

    const index = app.window.listSelection(app.list) orelse {
        gui.showMessage(.warning, window_title, "Select the realm you want to remove first.");
        return;
    };

    app.realms.remove(index) catch |err| switch (err) {
        error.NotFound => {
            gui.showMessage(.warning, window_title, "Select the realm you want to remove first.");
            return;
        },
        error.LastRealm => {
            gui.showMessage(.warning, window_title, "The launcher needs at least one realm.\n\n" ++
                "Type the address of another realm before removing this one.");
            return;
        },
        error.OutOfMemory => {
            gui.showMessage(.critical, window_title, "Not enough memory to remove the realm.");
            return;
        },
    };

    if (!writeRealms(app)) return;
    refreshRealmUi(app, .rows);
    setStatus(app, "Removed the realm; now using {s}", .{app.realms.activeHost()});
}

/// Write the current realm list to every realmlist file. Reports the failure
/// (usually a read-only game directory) and returns false when nothing changed.
fn writeRealms(app: *App) bool {
    app.client.write(app.io, &app.realms) catch |err| {
        var buffer: [512]u8 = undefined;
        const detail = std.fmt.bufPrint(
            &buffer,
            "Could not write {s} ({s}).\n\n" ++
                "If the game is in Program Files, move it somewhere the user can write to, " ++
                "or start the launcher as administrator.",
            .{ app.client.primary(), @errorName(err) },
        ) catch "Could not write realmlist.wtf.";
        gui.showMessage(.critical, window_title, detail);
        return false;
    };
    return true;
}

/// Show the active realm in the address field, the list and the friendly name.
/// `.rows` rebuilds the list entries (after a host was added or removed);
/// `.selection` only moves the highlight, which is safe during a list click.
fn refreshRealmUi(app: *App, mode: enum { rows, selection }) void {
    app.window.setEditText(app.edit, app.realms.activeHost()) catch {};
    switch (mode) {
        .rows => app.window.setListItems(app.list, app.realms.hosts.items, app.realms.active) catch {},
        .selection => app.window.setListSelection(app.list, app.realms.active),
    }
    app.window.setLabelText(app.realm_name, realmlist.displayName(app.realms.activeHost())) catch {};
}

fn setStatus(app: *App, comptime format: []const u8, args: anytype) void {
    var buffer: [512]u8 = undefined;
    const text = std.fmt.bufPrint(&buffer, format, args) catch return;
    app.window.setLabelText(app.status, text) catch {};
}
