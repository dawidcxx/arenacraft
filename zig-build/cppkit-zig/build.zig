const std = @import("std");
const cc = @import("./compile_commands.zig");
const file_queries = @import("file_queries.zig");
const include_dirs = @import("include_dirs.zig");

pub fn build(b: *std.Build) void {
    _ = b;
}

// querying for sources
pub const Ext = file_queries.Ext;
pub const Exts = struct {
    pub const JUST_C = file_queries.JUST_C;
    pub const JUST_CC = file_queries.JUST_CC;
    pub const JUST_CPP = file_queries.JUST_CPP;
    pub const JUST_H = file_queries.JUST_H;
};

pub const querySources = file_queries.querySources;
pub const SourceSet = file_queries.SourceSet;

// include path helpers
pub const addFlatIncludes = include_dirs.addFlatIncludes;

/// Options for `installHeadersDirectory` so emitted include trees only carry
/// real headers (the `.h`-only default misses `.hpp` etc). Without this,
/// whole source dirs would get copied and hashed into the trees, and since
/// consumers take `-I` on the tree, any source edit would churn every
/// downstream TU's flags.
pub const header_install_options: std.Build.Step.Compile.HeaderInstallation.Directory.Options = .{
    .include_extensions = &.{ ".h", ".hpp", ".hxx", ".inc", ".inl" },
};

// compile commands stuff
pub const addCompileCommands = cc.addCompileCommands;
pub const addCompileCommandsStep = cc.addCompileCommandsStep;
