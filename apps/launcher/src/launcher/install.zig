//! Locating the game installation.
//!
//! The launcher is installed *into* a World of Warcraft 3.3.5a directory, so
//! everything the launcher touches (`Wow.exe`, `Data`, `WTF`) is resolved
//! relative to the launcher's own executable, never to the current working
//! directory (which is whatever Explorer or a shortcut chose).

const std = @import("std");

/// Directory containing the running launcher, and therefore the game client.
///
/// On Windows the result is WTF-8 (see `std.process.executableDirPathAlloc`),
/// so a client in a non-ASCII directory still works. Owned by the caller.
pub fn dir(io: std.Io, allocator: std.mem.Allocator) ![]u8 {
    return std.process.executableDirPathAlloc(io, allocator);
}
