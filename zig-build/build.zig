const std = @import("std");
const builtin = @import("builtin");
const cpp = @import("./cppkit-zig/compile_commands.zig");
const AcGraph = @import("./BuildCommons.zig").AcGraph;

comptime {
    // The host-libc detection this build relies on is zig 0.16 behaviour;
    // keep in sync with Dockerfile ZIG_VERSION and flake.nix's zig_0_16.
    if (builtin.zig_version.major != 0 or builtin.zig_version.minor != 16)
        @compileError("arenacraft requires zig 0.16.x (Dockerfile ZIG_VERSION / flake.nix zig_0_16)");
}

pub fn runBuild(b: *std.Build) !void {
    const io = b.graph.io;
    var graph = AcGraph.create(b.allocator, io);
    defer graph.destroy();

    try graph.build(b);

    // after all targets exist so nothing is missed
    cpp.addCompileCommandsStep(b);
}
