//! Application state shared by the views and the helpers every view needs.
//!
//! The window shows one view at a time (`home_view`, `realms_view`). A view is
//! a function that declares its controls; switching destroys the previous
//! controls and records the other tree (`gui` replays the tree on resize).

const std = @import("std");
const gui = @import("gui");

const home_view = @import("home_view.zig");
const realmlist = @import("realmlist.zig");
const realms_view = @import("realms_view.zig");

pub const allocator = std.heap.smp_allocator;

pub const title = "Arenacraft Launcher";

/// Which view the window currently shows.
pub const View = enum { home, realms };

/// Taglines shown under the launcher title, picked once per start. They are
/// stored uppercase because the native `STATIC` has no letter-spacing control;
/// full caps give the line a badge look that separates it from the title.
pub const taglines = [_][]const u8{
    "WHERE THE CHALLENGE BEGINS",
    "WOTLK+ FOREVER ANNIVERSARY",
};

/// Choose the tagline for this process. The layout is rebuilt on every view
/// switch, so the choice is made once here and kept in `App` instead of being
/// re-rolled every time the realm switcher closes.
pub fn pickTagline(io: std.Io) []const u8 {
    var bytes: [2]u8 = undefined;
    io.random(&bytes);
    const value = std.mem.readInt(u16, &bytes, .little);
    return taglines[value % taglines.len];
}

/// State shared with the widget callbacks. `gui.run` keeps the stack frame in
/// `main` alive, so pointers into this struct stay valid until the window is
/// destroyed. The client and the committed realm list are borrowed from `main`;
/// the switcher's draft is owned here.
pub const App = struct {
    io: std.Io,
    window: *gui.Window,
    /// Game directory, owned by `main`.
    install_dir: []const u8,
    /// Borrowed from `main`; owns the resolved realmlist paths.
    client: *realmlist.Client,
    /// Borrowed from `main`; the committed list Launch Game writes.
    realms: *realmlist.List,
    view: View = .home,
    /// Tagline under the title, chosen once per start by `pickTagline`.
    tagline: []const u8,
    /// Controls of the realm view; reset by `showView` since the switch
    /// destroys them.
    switcher: realms_view.State = .{},
    /// Editable copy of `realms` while the switcher is open. Row picks,
    /// renames and removals only touch this draft; Apply writes it and adopts
    /// it, while Cancel or closing the window drops it.
    draft: ?realmlist.List = null,

    /// Replace the window content with `view`.
    pub fn showView(self: *App, view: View) void {
        self.window.clear();
        self.discardDraft();
        self.switcher = .{};
        self.view = view;

        // The switcher must not mutate the committed list while the player is
        // still deciding, so it starts from a deep copy.
        if (view == .realms) self.draft = self.realms.clone() catch unreachable;

        self.buildUi();
    }

    /// Drop the uncommitted draft. Called when leaving the switcher and when
    /// the process tears down with the switcher still open.
    pub fn deinit(self: *App) void {
        self.discardDraft();
    }

    fn discardDraft(self: *App) void {
        if (self.draft) |*list| {
            list.deinit();
            self.draft = null;
        }
    }

    /// Write the draft to every realmlist file, then adopt it as the committed
    /// list. Returns false (after the write alert) and keeps the draft in place
    /// when the write fails, so the player can retry or cancel.
    pub fn commitDraft(self: *App) bool {
        const draft = if (self.draft) |*list| list else return false;
        if (!self.writeList(draft)) return false;

        const committed = self.draft.?;
        self.draft = null;
        self.realms.deinit();
        self.realms.* = committed;
        return true;
    }

    fn buildUi(self: *App) void {
        const window = self.window;

        window.beginLayout();
        defer window.endLayout();

        window.container(.{
            .sizing = .grow_both,
            .padding = .all(20),
            .gap = 14,
            .direction = .top_to_bottom,
        });

        switch (self.view) {
            .home => home_view.build(self),
            .realms => realms_view.build(self),
        }

        window.endContainer();
    }

    /// Write the committed realm list to every realmlist file.
    pub fn writeRealms(self: *App) bool {
        return self.writeList(self.realms);
    }

    /// Write `list` to every realmlist file. Reports the failure (usually a
    /// read-only game directory) and returns false when nothing was written.
    fn writeList(self: *App, list: *const realmlist.List) bool {
        self.client.write(self.io, list) catch |err| {
            if (err == error.OutOfMemory) unreachable;
            alert(
                .critical,
                "Could not write {s} ({s}).\n\n" ++
                    "If the game is in Program Files, move it somewhere the user can write to, " ++
                    "or start the launcher as administrator.",
                .{ self.client.primary(), @errorName(err) },
            );
            return false;
        };
        return true;
    }
};

/// Show a message box with a formatted text. The launcher is a GUI-subsystem
/// binary, so this is how every failure reaches the player. When the text does
/// not fit the buffer the unformatted template is shown instead.
pub fn alert(kind: gui.MessageKind, comptime format: []const u8, args: anytype) void {
    var buffer: [512]u8 = undefined;
    const text = std.fmt.bufPrint(&buffer, format, args) catch format;
    gui.showMessage(kind, title, text);
}
