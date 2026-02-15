const std = @import("std");
// Import the local cppkit-zig build helper directly so edits in the workspace
// are used immediately instead of relying on Zig's package cache.
const cpp = @import("./cppkit-zig/build.zig");

var INCLUDE_PATH: []const u8 = undefined;

pub fn build(b: *std.Build) void {
    var env_map = std.process.getEnvMap(b.allocator) catch @panic("Failed to get environment variables");
    defer env_map.deinit();
    INCLUDE_PATH = env_map.get("FLAKE_INCLUDES") orelse @panic("missing FLAKE_INCLUDES");
}
