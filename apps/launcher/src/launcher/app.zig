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

/// State shared with the widget callbacks. `gui.run` keeps the stack frame in
/// `main` alive, so pointers into this struct stay valid until the window is
/// destroyed.
pub const App = struct {
    io: std.Io,
    window: *gui.Window,
    /// Game directory, owned by `main`.
    install_dir: []const u8,
    client: realmlist.Client,
    realms: realmlist.List,
    view: View = .home,
    /// Controls of the realm view; reset by `showView` since the switch
    /// destroys them.
    switcher: realms_view.State = .{},

    /// Replace the window content with `view`.
    pub fn showView(self: *App, view: View) void {
        self.window.clear();
        self.switcher = .{};
        self.view = view;

        self.buildUi() catch {
            alert(.critical, "Could not lay out the launcher window.", .{});
        };
    }

    fn buildUi(self: *App) !void {
        const window = self.window;

        try window.beginLayout();
        try window.container(.{
            .sizing = .grow_both,
            .padding = .all(20),
            .gap = 14,
            .direction = .top_to_bottom,
        });

        switch (self.view) {
            .home => try home_view.build(self),
            .realms => try realms_view.build(self),
        }

        try window.endContainer();
        try window.endLayout();
    }

    /// Write the current realm list to every realmlist file. Reports the
    /// failure (usually a read-only game directory) and returns false when
    /// nothing changed.
    pub fn writeRealms(self: *App) bool {
        self.client.write(self.io, &self.realms) catch |err| {
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
