const std = @import("std");
const cpp = @import("./cppkit-zig/compile_commands.zig");
const AcGraph = @import("./BuildCommons.zig").AcGraph;

pub fn runBuild(b: *std.Build) !void {
    const io = b.graph.io;
    var graph = AcGraph.create(b.allocator, io);
    defer graph.destroy();
    try graph.build(b);

    // after all targets exist so nothing is missed
    cpp.addCompileCommandsStep(b);
}
