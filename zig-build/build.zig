const std = @import("std");
const builtin = @import("builtin");
const cpp = @import("./cppkit-zig/compile_commands.zig");
const AcGraph = @import("./BuildCommons.zig").AcGraph;

pub fn runBuild(b: *std.Build) !void {
    const io = b.graph.io;
    var graph = AcGraph.create(b.allocator, io);
    defer graph.destroy();

    try graph.build(b);

    // Compile commands are opt-in: zig 0.17 has to walk the whole build graph
    // during the configure phase to produce them, which costs ~1.5s on every
    // `zig build` (the configure cache is poisoned, see BuildCommons.zig).
    // Only clangd/IDE users should pay for it:
    //   zig build compile-commands -Dclangd
    const clangd = b.option(bool, "clangd", "Generate compile_commands.json for clangd/IDE support") orelse false;
    if (clangd) {
        // after all targets exist so nothing is missed
        cpp.addCompileCommandsStep(b);
    } else {
        // Keep the step name resolvable so `zig build compile-commands` fails
        // with an explanation instead of "no step named 'compile-commands'".
        const step = b.step("compile-commands", "Generate compile_commands.json (requires -Dclangd)");
        step.dependOn(&b.addFail("compile_commands.json is opt-in: run `zig build compile-commands -Dclangd`").step);
    }
}
