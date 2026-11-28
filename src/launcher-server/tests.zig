//! Test entry point for the launcher server. The build registers this with
//! `zig build launcher-server-test`; it only pulls in the pure protocol,
//! command and session logic, not the listener.

test {
    _ = @import("protocol.zig");
    _ = @import("commands.zig");
    _ = @import("session.zig");
}
