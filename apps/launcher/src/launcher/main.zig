//! Entry point: the launcher's two views, the window and the callbacks.
//!
//! The window shows one view at a time: the home view (active realm, launch)
//! and the realm switcher. `gui` records the layout tree and replays it on
//! resize, so each view is a function that declares its controls; switching
//! views destroys the previous controls and records the other tree.
//!
//! The window is a Windows GUI (subsystem) binary, so failures before the
//! window exists are reported with a message box instead of stderr.

const std = @import("std");
const gui = @import("gui");

const game = @import("game.zig");
const install = @import("install.zig");
const realmlist = @import("realmlist.zig");

const allocator = std.heap.smp_allocator;

const window_title = "Arenacraft Launcher";

/// Which view the window currently shows.
const View = enum { home, realms };

/// How much of the realm list to rebuild after the active realm changed.
const Refresh = enum {
    /// Only move the list highlight. Safe inside the list's own notification.
    selection,
    /// Replace the rows (a host was added or removed).
    rows,
};

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
    view: View = .home,
    /// True while the Add button has started a new realm: Save then appends
    /// instead of editing the selected row.
    editing_new: bool = false,
    /// Controls of the current view. `showView` destroys them, so each is null
    /// unless the view that owns it was just built.
    list: ?*gui.ListBox = null,
    edit: ?*gui.Edit = null,
    realm_name: ?*gui.Label = null,
    active_realm: ?*gui.Label = null,
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
        .size = .{ .width = 640, .height = 480 },
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
    };

    showView(&app, .home);
    gui.run();
}

/// Replace the window content with `view`.
fn showView(app: *App, view: View) void {
    app.window.clear();
    app.list = null;
    app.edit = null;
    app.realm_name = null;
    app.active_realm = null;
    app.editing_new = false;
    app.view = view;

    buildUi(app) catch {
        gui.showMessage(.critical, window_title, "Could not lay out the launcher window.");
    };
}

fn buildUi(app: *App) !void {
    const window = app.window;

    try window.beginLayout();

    try window.container(.{
        .sizing = .{ .width = .{ .grow = .{} }, .height = .{ .grow = .{} } },
        .padding = .all(20),
        .gap = 14,
        .direction = .top_to_bottom,
    });

    switch (app.view) {
        .home => try buildHome(app),
        .realms => try buildRealms(app),
    }

    try window.endContainer();
    try window.endLayout();
}

/// Home: the active realm sits in the bottom-left corner, the game actions on
/// the right.
fn buildHome(app: *App) !void {
    const window = app.window;

    _ = try window.addLabel(.{ .text = "Arenacraft", .font = .title });
    _ = try window.addLabel(.{ .text = "World of Warcraft 3.3.5a" });

    // Spacer: pushes the bar to the bottom edge.
    try window.container(.{ .sizing = .{ .height = .{ .grow = .{} } } });
    try window.endContainer();

    try window.container(.{
        .sizing = .{ .width = .{ .grow = .{} } },
        .gap = 10,
        .alignment = .{ .y = .center },
    });
    app.active_realm = try window.addLabel(.{ .text = app.realms.activeHost(), .wrap = .none });
    _ = try window.addButton(.{
        .text = "Change Realm",
        .sizing = .{ .width = .{ .fixed = 120 }, .height = .{ .fixed = 30 } },
        .onClick = .{ .function = onOpenRealms, .context = app },
    });
    // Spacer: the actions stay on the right edge.
    try window.container(.{ .sizing = .{ .width = .{ .grow = .{} } } });
    try window.endContainer();
    _ = try window.addButton(.{
        .text = "Launch Game",
        .sizing = .{ .width = .{ .fixed = 140 }, .height = .{ .fixed = 30 } },
        .onClick = .{ .function = onLaunchGame, .context = app },
    });
    _ = try window.addButton(.{
        .text = "Quit",
        .sizing = .{ .width = .{ .fixed = 70 }, .height = .{ .fixed = 30 } },
        .onClick = .{ .function = onQuit, .context = app },
    });
    try window.endContainer();
}

/// Realm switcher: remembered realms with Add and Remove underneath, the
/// selected realm's address with Save on the right, Back at the bottom.
fn buildRealms(app: *App) !void {
    const window = app.window;

    _ = try window.addLabel(.{ .text = "Realms", .font = .title });

    try window.container(.{
        .sizing = .{ .width = .{ .grow = .{} }, .height = .{ .grow = .{} } },
        .gap = 16,
    });

    try window.container(.{
        .sizing = .{ .width = .{ .fixed = 240 }, .height = .{ .grow = .{} } },
        .gap = 8,
        .direction = .top_to_bottom,
    });
    app.list = try window.addListBox(.{
        .items = app.realms.hosts.items,
        .selected = app.realms.active,
        .onSelect = .{ .function = onRealmSelect, .context = app },
        .sizing = .{
            .width = .{ .grow = .{} },
            .height = .{ .grow = .{} },
        },
    });
    try window.container(.{
        .sizing = .{ .width = .{ .grow = .{} } },
        .gap = 8,
        .alignment = .{ .y = .center },
    });
    _ = try window.addButton(.{
        .text = "Add",
        .sizing = .{ .width = .{ .fixed = 70 }, .height = .{ .fixed = 28 } },
        .onClick = .{ .function = onRealmAdd, .context = app },
    });
    _ = try window.addButton(.{
        .text = "Remove",
        .sizing = .{ .width = .{ .fixed = 90 }, .height = .{ .fixed = 28 } },
        .onClick = .{ .function = onRealmRemove, .context = app },
    });
    try window.endContainer();
    try window.endContainer();

    try window.container(.{
        .sizing = .{ .width = .{ .grow = .{} }, .height = .{ .grow = .{} } },
        .gap = 8,
        .direction = .top_to_bottom,
    });
    _ = try window.addLabel(.{ .text = "Realm address" });
    try window.container(.{
        .sizing = .{ .width = .{ .grow = .{} } },
        .gap = 8,
        .alignment = .{ .y = .center },
    });
    app.edit = try window.addEdit(.{
        .text = app.realms.activeHost(),
        .onEnter = .{ .function = onRealmSave, .context = app },
        .sizing = .{
            .width = .{ .grow = .{} },
            .height = .{ .fixed = 26 },
        },
    });
    _ = try window.addButton(.{
        .text = "Save",
        .sizing = .{ .width = .{ .fixed = 70 }, .height = .{ .fixed = 26 } },
        .onClick = .{ .function = onRealmSave, .context = app },
    });
    try window.endContainer();
    app.realm_name = try window.addLabel(.{ .text = realmlist.displayName(app.realms.activeHost()) });
    try window.endContainer();

    try window.endContainer();

    try window.container(.{ .sizing = .{ .width = .{ .grow = .{} } } });
    _ = try window.addButton(.{
        .text = "Back",
        .sizing = .{ .width = .{ .fixed = 90 }, .height = .{ .fixed = 30 } },
        .onClick = .{ .function = onBack, .context = app },
    });
    try window.endContainer();
}

fn onOpenRealms(context: ?*anyopaque) void {
    const app: *App = @ptrCast(@alignCast(context orelse return));
    showView(app, .realms);
}

/// Leaving the switcher keeps what was already saved; a draft address is only
/// committed with Save (or Enter).
fn onBack(context: ?*anyopaque) void {
    const app: *App = @ptrCast(@alignCast(context orelse return));
    showView(app, .home);
}

/// Picking a row makes it the active realm and loads its address into the
/// field, ready to be edited and saved.
fn onRealmSelect(context: ?*anyopaque) void {
    const app: *App = @ptrCast(@alignCast(context orelse return));
    const list = app.list orelse return;
    const edit = app.edit orelse return;

    const index = app.window.listSelection(list) orelse return;
    if (index >= app.realms.hosts.items.len) return;

    const host = app.realms.hosts.items[index];
    _ = app.realms.setActive(host) catch return;
    app.editing_new = false;
    app.window.setEditText(edit, host) catch return;

    _ = writeRealms(app);
    refreshRealmUi(app, .selection);
}

/// Start a new realm: clear the field and the list highlight, so Save adds a
/// new entry instead of editing the selected one.
fn onRealmAdd(context: ?*anyopaque) void {
    const app: *App = @ptrCast(@alignCast(context orelse return));

    app.editing_new = true;
    if (app.list) |list| app.window.clearListSelection(list);
    if (app.edit) |edit| {
        app.window.setEditText(edit, "") catch {};
        app.window.focusEdit(edit);
    }
    if (app.realm_name) |label| app.window.setLabelText(label, "New realm") catch {};
}

/// Save: apply the address to the selected realm, or add it as a new realm
/// when Add started a draft.
fn onRealmSave(context: ?*anyopaque) void {
    const app: *App = @ptrCast(@alignCast(context orelse return));
    const edit = app.edit orelse return;

    const text = app.window.editText(edit) catch {
        gui.showMessage(.critical, window_title, "Not enough memory to read the address field.");
        return;
    };
    defer allocator.free(text);

    const host = realmlist.validate(text) orelse {
        gui.showMessage(.warning, window_title, if (text.len == 0)
            "Type the address of the realm you want to add."
        else
            "That is not a realm address.\n\n" ++
                "Type a logon host such as logon.arenacraft.fun " ++
                "(letters, digits, dots and dashes only).");
        return;
    };

    // A highlighted row means the field is editing that realm; no row means
    // Add started a new one.
    const selected: ?usize = if (app.editing_new)
        null
    else if (app.list) |list|
        app.window.listSelection(list)
    else
        null;

    if (selected) |index| {
        app.realms.rename(index, host) catch |err| switch (err) {
            error.Duplicate => {
                gui.showMessage(.warning, window_title, "That realm is already in the list.");
                return;
            },
            error.NotFound => {
                _ = app.realms.setActive(host) catch return;
            },
            error.OutOfMemory => {
                gui.showMessage(.critical, window_title, "Not enough memory to save the realm.");
                return;
            },
        };
    } else {
        _ = app.realms.setActive(host) catch {
            gui.showMessage(.critical, window_title, "Not enough memory to remember the realm.");
            return;
        };
    }

    app.editing_new = false;
    _ = writeRealms(app);
    refreshRealmUi(app, .rows);
    app.window.setEditText(edit, app.realms.activeHost()) catch {};
}

fn onLaunchGame(context: ?*anyopaque) void {
    const app: *App = @ptrCast(@alignCast(context orelse return));

    // Nothing is written at startup, so a fresh installation gets its file
    // here even if the player never opened the switcher.
    if (!writeRealms(app)) return;

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

/// The player pressed Remove: forget the highlighted realm. If it was the
/// active one, the next remembered realm becomes active and is written.
fn onRealmRemove(context: ?*anyopaque) void {
    const app: *App = @ptrCast(@alignCast(context orelse return));
    const list = app.list orelse return;

    const index = app.window.listSelection(list) orelse {
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

    _ = writeRealms(app);
    app.editing_new = false;
    refreshRealmUi(app, .rows);
    if (app.edit) |edit| app.window.setEditText(edit, app.realms.activeHost()) catch {};
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

/// Show the active realm in the controls of the current view. The add field is
/// cleared by its callback, not here.
fn refreshRealmUi(app: *App, mode: Refresh) void {
    const host = app.realms.activeHost();

    if (app.list) |list| switch (mode) {
        .rows => app.window.setListItems(list, app.realms.hosts.items, app.realms.active) catch {},
        .selection => app.window.setListSelection(list, app.realms.active),
    };
    if (app.realm_name) |label| app.window.setLabelText(label, realmlist.displayName(host)) catch {};
    if (app.active_realm) |label| app.window.setLabelText(label, host) catch {};
}
