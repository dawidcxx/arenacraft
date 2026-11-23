//! Home view: the active realm sits in the bottom-left corner, the game
//! actions on the right.

const gui = @import("gui");

const app_mod = @import("app.zig");
const game = @import("game.zig");

const App = app_mod.App;

pub fn build(app: *App) !void {
    const window = app.window;

    _ = try window.addLabel(.{ .text = "Arenacraft", .font = .title });
    _ = try window.addLabel(.{ .text = "World of Warcraft 3.3.5a" });

    try window.spacer(.top_to_bottom);

    try window.container(.{
        .sizing = .grow_width,
        .gap = 10,
        .alignment = .{ .y = .center },
    });
    _ = try window.addLabel(.{ .text = app.realms.activeHost(), .wrap = .none });
    _ = try window.addButton(.{
        .text = "Change Realm",
        .sizing = .fixed(120, 30),
        .onClick = .bind(app, onChangeRealm),
    });
    try window.spacer(.left_to_right);
    _ = try window.addButton(.{
        .text = "Launch Game",
        .sizing = .fixed(140, 30),
        .onClick = .bind(app, onLaunch),
    });
    _ = try window.addButton(.{
        .text = "Quit",
        .sizing = .fixed(70, 30),
        .onClick = .bind(app, onQuit),
    });
    try window.endContainer();
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
