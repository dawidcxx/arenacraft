//! Starting the game client that sits next to the launcher.
//!
//! `launchWow` takes the installation directory (see `install.zig`) rather than
//! resolving it again, so the launcher and the realmlist code always agree on
//! which client they are talking to.

const std = @import("std");
const lib = @import("lib");
const c = lib.win32.c;

pub const Outcome = union(enum) {
    started,
    /// Human-readable reason the game could not be started. Allocated with the
    /// caller's allocator.
    failed: []u8,
};

/// Spawn `Wow.exe` from `install_dir`, with that directory as the child's
/// working directory (WoW resolves `Data`, `WTF`, `Logs` and `Cache` relatively).
pub fn launchWow(allocator: std.mem.Allocator, install_dir: []const u8) error{OutOfMemory}!Outcome {
    const wow_path = try std.fs.path.join(allocator, &.{ install_dir, "Wow.exe" });
    defer allocator.free(wow_path);

    // Paths from the OS are WTF-8 (see std.process.executablePath), so convert
    // as WTF-8: a client in a non-ASCII directory still starts.
    const wow_path_w = try toWide(allocator, wow_path);
    defer allocator.free(wow_path_w);
    const exe_dir_w = try toWide(allocator, install_dir);
    defer allocator.free(exe_dir_w);

    if (c.GetFileAttributesW(wow_path_w.ptr) == std.math.maxInt(c.DWORD)) {
        return .{ .failed = try std.fmt.allocPrint(
            allocator,
            "Wow.exe was not found next to the launcher.\n\n{s}\n\n" ++
                "Install Arenacraft into your World of Warcraft 3.3.5a folder, " ++
                "the one that contains Wow.exe.",
            .{wow_path},
        ) };
    }

    std.log.info("starting {s}", .{wow_path});

    var startup: c.STARTUPINFOW = std.mem.zeroes(c.STARTUPINFOW);
    startup.cb = @sizeOf(c.STARTUPINFOW);
    var process_info: c.PROCESS_INFORMATION = undefined;

    // lpCommandLine is null: with lpApplicationName set, Windows derives the
    // command line from the application path, so no manual quoting is needed.
    const ok = c.CreateProcessW(
        wow_path_w.ptr,
        null,
        null,
        null,
        c.FALSE,
        0,
        null,
        exe_dir_w.ptr,
        &startup,
        &process_info,
    );
    if (ok == 0) {
        return .{ .failed = try std.fmt.allocPrint(
            allocator,
            "Wow.exe could not be started from:\n\n{s}\n\nWindows error {d}.",
            .{ install_dir, c.GetLastError() },
        ) };
    }

    // The game outlives the launcher; release our handles instead of waiting.
    _ = c.CloseHandle(process_info.hProcess);
    _ = c.CloseHandle(process_info.hThread);
    return .started;
}

fn toWide(allocator: std.mem.Allocator, value: []const u8) error{OutOfMemory}![:0]u16 {
    return std.unicode.wtf8ToWtf16LeAllocZ(allocator, value) catch |err| switch (err) {
        error.OutOfMemory => error.OutOfMemory,
        // A path the OS itself produced cannot contain invalid WTF-8; treat a
        // violation as a bug instead of a user-facing branch.
        error.InvalidWtf8 => unreachable,
    };
}
