const std = @import("std");
const buildImpl = @import("./zig-build/build.zig");

pub fn build(b: *std.Build) void {
    buildImpl.runBuild(b) catch |e| {
        std.debug.panic("Failed to execute build due to: '{}'", .{e});
    };
}
