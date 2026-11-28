//! `ac launcher-server` - TCP RPC front door for the launcher.
//!
//! The wire format lives in `protocol.zig`; command handlers in `commands.zig`.
//! The listener and every client run on the new `std.Io` interface; clients
//! are tasks in an `std.Io.Group`, so they are handled concurrently and are
//! all torn down when the accept loop ends.

const std = @import("std");

const commands = @import("commands.zig");
const protocol = @import("protocol.zig");
const Session = @import("session.zig").Session;

/// Mirrors `LauncherServerConfig` in `LauncherServer.h` (parsed by `Cli.cpp`
/// on the C++ side and passed in through the C ABI below).
pub const Config = extern struct {
    port: u16,
    /// NUL-terminated directory exposed by `get_file`, or null to disable it.
    root: ?[*:0]const u8,
};

comptime {
    std.debug.assert(@offsetOf(Config, "root") == @alignOf(usize));
}

/// std.Io.Threaded allocates its task/thread bookkeeping from here from worker
/// threads, so it must be thread-safe.
const allocator = std.heap.smp_allocator;

/// Each client is an OS thread with `std.Io.Threaded`'s default concurrency.
/// Cap it so a connection flood cannot spawn threads without bound. Reaching
/// the cap is fatal (see the accept loop): silently dropping connections
/// would hide overload and leave the launcher staring at a reset socket, and
/// `std.Io.Threaded` reports the same error for OOM/thread-spawn failures.
const max_clients = 1024;

fn handleClient(io: std.Io, stream: std.Io.net.Stream, root: ?std.Io.Dir) std.Io.Cancelable!void {
    defer stream.close(io);

    var read_buffer: [protocol.max_header_len]u8 = undefined;
    var reader = stream.reader(io, &read_buffer);
    var write_buffer: [16 * 1024]u8 = undefined;
    var writer = stream.writer(io, &write_buffer);

    var session = Session.init(io, &reader.interface, &writer.interface, allocator);
    defer session.deinit();

    while (true) {
        const request = session.next() catch {
            if (reader.err) |read_err| if (read_err == error.Canceled) return error.Canceled;
            // Malformed/oversized frames and unsupported bodies close the
            // connection; read failures (EOF/reset) are normal.
            return;
        } orelse return;

        const response = commands.handle(.{ .root = root }, io, request, session.allocator());
        session.reply(request, response) catch {
            if (writer.err) |write_err| if (write_err == error.Canceled) return error.Canceled;
            // A partial response can no longer be framed reliably.
            return;
        };
    }
}

fn serve(config: Config) !void {
    var threaded: std.Io.Threaded = .init(allocator, .{
        .concurrent_limit = .limited(max_clients),
    });
    defer threaded.deinit();
    const io = threaded.io();

    const root_path: ?[]const u8 = if (config.root) |root| std.mem.span(root) else null;
    var root_dir: ?std.Io.Dir = null;
    if (root_path) |path| {
        if (path.len != 0) {
            root_dir = if (std.fs.path.isAbsolute(path))
                try std.Io.Dir.openDirAbsolute(io, path, .{})
            else
                try std.Io.Dir.cwd().openDir(io, path, .{});
            std.log.info("launcher-server serving files from {s}", .{path});
        }
    }
    defer if (root_dir) |dir| dir.close(io);
    if (root_dir == null) std.log.info("launcher-server file serving disabled (pass --root)", .{});

    const address: std.Io.net.IpAddress = .{ .ip4 = .unspecified(config.port) };
    // On POSIX this sets SO_REUSEADDR *and* SO_REUSEPORT. SO_REUSEADDR lets a
    // restart bind immediately while old connections sit in TIME_WAIT; the
    // SO_REUSEPORT side effect means a second instance would share the port
    // (both serve requests) instead of failing. Revisit once the server owns
    // mutable state.
    var server = try address.listen(io, .{ .reuse_address = true });
    defer server.deinit(io);

    std.log.info("launcher-server listening on 0.0.0.0:{d}", .{config.port});

    var group: std.Io.Group = .init;
    // Cancels (and waits for) every in-flight client before we tear the
    // listener, root directory and Io implementation down.
    defer group.cancel(io);

    while (true) {
        const stream = server.accept(io) catch |err| switch (err) {
            error.Canceled => return,
            else => return err,
        };

        // One task per client. `ConcurrencyUnavailable` means either the
        // `max_clients` cap or resource exhaustion (OOM, thread spawn): the
        // server can no longer hold up its end, so fail loudly instead of
        // silently dropping the connection. At this point nothing was spawned,
        // so there is no group state to unwind.
        group.concurrent(io, handleClient, .{ io, stream, root_dir }) catch |err| switch (err) {
            error.ConcurrencyUnavailable => @panic("launcher-server: unable to spawn a client task"),
        };
    }
}

/// C ABI entry point called by `launcher_server_main` in `Cli.cpp`.
export fn launcherServerRun(config: ?*const Config) c_int {
    const cfg = config orelse return 2;

    serve(cfg.*) catch |err| {
        std.log.err("launcher-server stopped: {s}", .{@errorName(err)});
        return 1;
    };
    return 0;
}
