//! Realm switcher: a narrow column. The realm list fills the top, Add and
//! Remove sit underneath it, then the selected realm's address, and Apply and
//! Cancel close the column at the bottom.
//!
//! The view never touches the committed realm list: `App.showView` gives it a
//! deep copy (`App.draft`). Row picks, renames and removals edit that copy, and
//! only Apply writes it and adopts it. Cancel returns to the home view with the
//! draft dropped, so nothing the player did here can leak into the realmlist.

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
    /// True while Add has started a new realm: Apply then appends instead of
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

/// The view's editable copy of the realm list. Only null when the view is not
/// showing; `App.showView` clones the list before calling `build`.
fn editList(app: *App) ?*realmlist.List {
    return if (app.draft) |*list| list else null;
}

pub fn build(app: *App) void {
    const window = app.window;
    const ui = &app.switcher;
    const draft = editList(app) orelse unreachable;

    _ = window.addLabel(.{ .text = "Realms", .font = .title, .wrap = .none });

    // The list takes every pixel the fixed controls below do not need.
    ui.list = window.addListBox(.{
        .items = draft.hosts.items,
        .selected = draft.active,
        .onSelect = .bind(app, onSelect),
        .sizing = .grow_both,
    });

    // Row actions for the list.
    window.container(.{ .sizing = .grow_width, .gap = 8 });
    _ = window.addButton(.{ .text = "Add", .sizing = .fixedHeight(32), .onClick = .bind(app, onAdd) });
    _ = window.addButton(.{ .text = "Remove", .sizing = .fixedHeight(32), .onClick = .bind(app, onRemove) });
    window.endContainer();

    // Address of the selected realm, editable in place.
    _ = window.addLabel(.{ .text = "Realm address", .wrap = .none });
    ui.edit = window.addEdit(.{
        .text = draft.activeHost(),
        .onEnter = .bind(app, onSave),
        .sizing = .fixedHeight(30),
    });
    ui.realm_name = window.addLabel(.{
        .text = realmlist.displayName(draft.activeHost()),
        .wrap = .none,
    });

    // Commit and navigation, full width like the home actions.
    window.container(.{
        .sizing = .grow_width,
        .gap = 8,
        .direction = .top_to_bottom,
    });
    _ = window.addButton(.{ .text = "Apply", .sizing = .fixedHeight(44), .onClick = .bind(app, onSave) });
    _ = window.addButton(.{ .text = "Cancel", .sizing = .fixedHeight(44), .onClick = .bind(app, onCancel) });
    window.endContainer();
}

/// Cancel returns to the home view without committing anything. Row picks,
/// renames and removals only touched the draft, so dropping it restores the
/// committed list exactly.
fn onCancel(app: *App) void {
    app.showView(.home);
}

/// Picking a row makes it the draft's active realm and loads its address into
/// the field. Nothing is written until Apply.
fn onSelect(app: *App) void {
    const ui = &app.switcher;
    const list = ui.list orelse return;
    const edit = ui.edit orelse return;
    const draft = editList(app) orelse return;

    const index = app.window.listSelection(list) orelse return;
    if (index >= draft.hosts.items.len) return;

    const host = draft.hosts.items[index];
    _ = draft.setActive(host) catch unreachable;
    ui.editing_new = false;
    app.window.setEditText(edit, host);

    refresh(app, .selection);
}

/// Start a new realm: clear the field and the list highlight, so Apply adds a
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

/// Apply: commit the address to the selected realm (or append it as a new
/// realm when Add started a draft), write the draft and return home.
fn onSave(app: *App) void {
    const ui = &app.switcher;
    const edit = ui.edit orelse return;
    const draft = editList(app) orelse return;

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
        draft.rename(index, host) catch |err| switch (err) {
            error.Duplicate => {
                alert(.warning, "That realm is already in the list.", .{});
                return;
            },
            error.NotFound => {
                _ = draft.setActive(host) catch unreachable;
            },
            error.OutOfMemory => unreachable,
        };
    } else {
        _ = draft.setActive(host) catch unreachable;
    }

    ui.editing_new = false;
    // A failed write already showed an alert; show the pending draft and stay
    // so the player can fix the directory, retry, or cancel the whole edit.
    if (!app.commitDraft()) {
        refresh(app, .rows);
        return;
    }

    // Apply also closes the switcher: the home view is the confirmation that
    // the new address is the one the launch will use.
    app.showView(.home);
}

/// Forget the highlighted realm in the draft. Apply persists the removal;
/// Cancel leaves the committed list untouched.
fn onRemove(app: *App) void {
    const ui = &app.switcher;
    const list = ui.list orelse return;
    const draft = editList(app) orelse return;

    const index = app.window.listSelection(list) orelse {
        alert(.warning, "Select the realm you want to remove first.", .{});
        return;
    };

    draft.remove(index) catch |err| switch (err) {
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

    ui.editing_new = false;
    refresh(app, .rows);
    if (ui.edit) |edit| app.window.setEditText(edit, draft.activeHost());
}

/// Show the draft's active realm in the controls. The address field is updated
/// by the callers, not here.
fn refresh(app: *App, mode: Refresh) void {
    const ui = &app.switcher;
    const draft = editList(app) orelse return;

    if (ui.list) |list| switch (mode) {
        .rows => app.window.setListItems(list, draft.hosts.items, draft.active),
        .selection => app.window.setListSelection(list, draft.active),
    };
    if (ui.realm_name) |label| {
        app.window.setLabelText(label, realmlist.displayName(draft.activeHost()));
    }
}
