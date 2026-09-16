//! Public API of the compile_commands.json generator.
//!
//! All implementation lives in ./CompileCommandsImpl.zig (with the entry
//! database in ./CompileCommandsDb.zig), this file only instantiates the
//! process wide state and re-exports the entry points.

const std = @import("std");
const impl = @import("./CompileCommandsImpl.zig");

/// Process wide generator state backing the public API below.
var global: impl.CompileCommands = .{};

/// Mark a compile step so its sources and flags end up in compile_commands.json.
pub fn addCompileCommands(target: *std.Build.Step.Compile) void {
    global.addCompileCommands(target);
}

/// Register the "compile-commands" build step.
///
/// NOTE: must be called after all targets have been created and had their
/// include paths / source files configured, otherwise they will be missed.
pub fn addCompileCommandsStep(b: *std.Build) void {
    global.addCompileCommandsStep(b);
}
