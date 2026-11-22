const std = @import("std");
const lib = @import("lib");

pub fn main() void {
    std.log.info("launcher starting (win32)", .{});

    lib.win32.messageBox(
        "Arenacraft Launcher",
        "win32 build is alive.\n\nwindows.h was imported with @cImport and MessageBoxA works.",
    );
}
