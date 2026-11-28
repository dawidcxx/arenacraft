//! Home view: a compact, mobile-style column. The title and tagline sit at the
//! top, the game actions are full-width buttons stacked above the bottom edge,
//! and the realm the launch will use sits in the bottom-left corner as small
//! secondary text.
//!
//! The window is narrow on purpose: every action is a single column, so the
//! player never has to scan a row of differently sized buttons.

const gui = @import("gui");

const app_mod = @import("app.zig");
const game = @import("game.zig");

const App = app_mod.App;

/// Height of the action buttons. Generous enough to read as a phone launcher
/// button instead of a dialog control.
const action_height = 48;

pub fn build(app: *App) void {
    const window = app.window;

    // Header: the product name and a tagline picked for this start.
    window.container(.{
        .sizing = .grow_width,
        .gap = 2,
        .direction = .top_to_bottom,
        .alignment = .{ .x = .center },
    });
    _ = window.addLabel(.{ .text = "Arenacraft Launcher", .font = .title, .wrap = .none });
    _ = window.addLabel(.{ .text = app.tagline, .font = .small, .wrap = .none });
    window.endContainer();

    // Keep the actions on the bottom edge, like a phone's home screen.
    window.spacer(.top_to_bottom);

    // Actions, stacked vertically and sharing the full width.
    window.container(.{
        .sizing = .grow_width,
        .gap = 10,
        .direction = .top_to_bottom,
    });
    _ = window.addButton(.{
        .text = "Launch Game",
        .sizing = .fixedHeight(action_height),
        .onClick = .bind(app, onLaunch),
    });
    _ = window.addButton(.{
        .text = "Change Realm",
        .sizing = .fixedHeight(action_height),
        .onClick = .bind(app, onChangeRealm),
    });
    _ = window.addButton(.{
        .text = "Quit",
        .sizing = .fixedHeight(action_height),
        .onClick = .bind(app, onQuit),
    });
    window.endContainer();

    // Footer: the realm the launch will connect to, small and left aligned.
    // Both labels use `.wrap = .none`, so a long address is clipped instead of
    // breaking onto a second line.
    window.container(.{
        .sizing = .grow_width,
        .gap = 4,
        .direction = .left_to_right,
        .alignment = .{ .y = .center },
    });
    _ = window.addLabel(.{ .text = "realm:", .font = .small, .wrap = .none });
    _ = window.addLabel(.{ .text = app.realms.activeHost(), .font = .small_bold, .wrap = .none });
    window.endContainer();
}

fn onChangeRealm(app: *App) void {
    app.showView(.realms);
}

fn onLaunch(app: *App) void {
    // Nothing is written at startup, so a fresh installation gets its file
    // here even if the player never opened the switcher.
    if (!app.writeRealms()) return;

    const outcome = game.launchWow(app_mod.allocator, app.install_dir) catch unreachable;

    switch (outcome) {
        .started => app.window.close(),
        .failed => |text| {
            defer app_mod.allocator.free(text);
            gui.showMessage(.critical, app_mod.title, text);
        },
    }
}

fn onQuit(app: *App) void {
    app.window.close();
}
