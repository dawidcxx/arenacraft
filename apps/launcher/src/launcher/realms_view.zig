//! Realm switcher: remembered realms with Add and Remove underneath, the
//! selected realm's address with Save on the right, Back at the bottom.

const gui = @import("gui");

const app_mod = @import("app.zig");
const realmlist = @import("realmlist.zig");

const App = app_mod.App;
const alert = app_mod.alert;

/// Controls of this view. Null unless the view is showing: `App.showView`
/// resets the state, and the controls are destroyed with the view.
pub const State = struct {
    list: ?*gui.ListBox = null,
    edit: ?*gui.Edit = null,
    realm_name: ?*gui.Label = null,
    /// True while Add has started a new realm: Save then appends instead of
    /// editing the selected row.
    editing_new: bool = false,
};

/// How much of the realm list to rebuild after the active realm changed.
const Refresh = enum {
    /// Only move the list highlight. Safe inside the list's own notification.
    selection,
    /// Replace the rows (a host was added or removed).
    rows,
};

pub fn build(app: *App) !void {
    const window = app.window;
    const ui = &app.switcher;

    _ = try window.addLabel(.{ .text = "Realms", .font = .title });

    try window.container(.{ .sizing = .grow_both, .gap = 16 });

    // Left: the remembered realms and their actions.
    try window.container(.{ .sizing = .fixedWidth(240), .gap = 8, .direction = .top_to_bottom });
    ui.list = try window.addListBox(.{
        .items = app.realms.hosts.items,
        .selected = app.realms.active,
        .onSelect = .bind(app, onSelect),
        .sizing = .grow_both,
    });
    try window.container(.{ .sizing = .grow_width, .gap = 8, .alignment = .{ .y = .center } });
    _ = try window.addButton(.{ .text = "Add", .sizing = .fixed(70, 28), .onClick = .bind(app, onAdd) });
    _ = try window.addButton(.{ .text = "Remove", .sizing = .fixed(90, 28), .onClick = .bind(app, onRemove) });
    try window.endContainer();
    try window.endContainer();

    // Right: the address of the selected realm.
    try window.container(.{ .sizing = .grow_both, .gap = 8, .direction = .top_to_bottom });
    _ = try window.addLabel(.{ .text = "Realm address" });
    try window.container(.{ .sizing = .grow_width, .gap = 8, .alignment = .{ .y = .center } });
    ui.edit = try window.addEdit(.{
        .text = app.realms.activeHost(),
        .onEnter = .bind(app, onSave),
        .sizing = .fixedHeight(26),
    });
    _ = try window.addButton(.{ .text = "Save", .sizing = .fixed(70, 26), .onClick = .bind(app, onSave) });
    try window.endContainer();
    ui.realm_name = try window.addLabel(.{ .text = realmlist.displayName(app.realms.activeHost()) });
    try window.endContainer();

    try window.endContainer();

    try window.container(.{ .sizing = .grow_width });
    _ = try window.addButton(.{ .text = "Back", .sizing = .fixed(90, 30), .onClick = .bind(app, onBack) });
    try window.endContainer();
}

/// Leaving the switcher keeps what was already saved; a draft address is only
/// committed with Save (or Enter).
fn onBack(app: *App) void {
    app.showView(.home);
}

/// Picking a row makes it the active realm and loads its address into the
/// field, ready to be edited and saved.
fn onSelect(app: *App) void {
    const ui = &app.switcher;
    const list = ui.list orelse return;
    const edit = ui.edit orelse return;

    const index = app.window.listSelection(list) orelse return;
    if (index >= app.realms.hosts.items.len) return;

    const host = app.realms.hosts.items[index];
    _ = app.realms.setActive(host) catch unreachable;
    ui.editing_new = false;
    app.window.setEditText(edit, host);

    _ = app.writeRealms();
    refresh(app, .selection);
}

/// Start a new realm: clear the field and the list highlight, so Save adds a
/// new entry instead of editing the selected one.
fn onAdd(app: *App) void {
    const ui = &app.switcher;

    ui.editing_new = true;
    if (ui.list) |list| app.window.clearListSelection(list);
    if (ui.edit) |edit| {
        app.window.setEditText(edit, "");
        app.window.focusEdit(edit);
    }
    if (ui.realm_name) |label| app.window.setLabelText(label, "New realm");
}

/// Save: apply the address to the selected realm, or add it as a new realm
/// when Add started a draft.
fn onSave(app: *App) void {
    const ui = &app.switcher;
    const edit = ui.edit orelse return;

    const text = app.window.editText(edit);
    defer app_mod.allocator.free(text);

    const host = realmlist.validate(text) orelse {
        if (text.len == 0) {
            alert(.warning, "Type the address of the realm you want to add.", .{});
        } else {
            alert(
                .warning,
                "That is not a realm address.\n\n" ++
                    "Type a logon host such as logon.arenacraft.fun " ++
                    "(letters, digits, dots and dashes only).",
                .{},
            );
        }
        return;
    };

    // A highlighted row means the field is editing that realm; no row means
    // Add started a new one.
    const selected: ?usize = if (ui.editing_new or ui.list == null)
        null
    else
        app.window.listSelection(ui.list.?);

    if (selected) |index| {
        app.realms.rename(index, host) catch |err| switch (err) {
            error.Duplicate => {
                alert(.warning, "That realm is already in the list.", .{});
                return;
            },
            error.NotFound => {
                _ = app.realms.setActive(host) catch unreachable;
            },
            error.OutOfMemory => unreachable,
        };
    } else {
        _ = app.realms.setActive(host) catch unreachable;
    }

    ui.editing_new = false;
    _ = app.writeRealms();
    refresh(app, .rows);
    app.window.setEditText(edit, app.realms.activeHost());
}

/// Forget the highlighted realm. If it was the active one, the next
/// remembered realm becomes active and is written.
fn onRemove(app: *App) void {
    const ui = &app.switcher;
    const list = ui.list orelse return;

    const index = app.window.listSelection(list) orelse {
        alert(.warning, "Select the realm you want to remove first.", .{});
        return;
    };

    app.realms.remove(index) catch |err| switch (err) {
        error.NotFound => {
            alert(.warning, "Select the realm you want to remove first.", .{});
            return;
        },
        error.LastRealm => {
            alert(
                .warning,
                "The launcher needs at least one realm.\n\n" ++
                    "Type the address of another realm before removing this one.",
                .{},
            );
            return;
        },
        error.OutOfMemory => unreachable,
    };

    _ = app.writeRealms();
    ui.editing_new = false;
    refresh(app, .rows);
    if (ui.edit) |edit| app.window.setEditText(edit, app.realms.activeHost());
}

/// Show the active realm in the controls. The address field is updated by the
/// callers, not here.
fn refresh(app: *App, mode: Refresh) void {
    const ui = &app.switcher;

    if (ui.list) |list| switch (mode) {
        .rows => app.window.setListItems(list, app.realms.hosts.items, app.realms.active),
        .selection => app.window.setListSelection(list, app.realms.active),
    };
    if (ui.realm_name) |label| {
        app.window.setLabelText(label, realmlist.displayName(app.realms.activeHost()));
    }
}
