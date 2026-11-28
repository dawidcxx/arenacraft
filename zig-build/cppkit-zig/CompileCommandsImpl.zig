//! compile_commands.json generation from compile steps marked with
//! addCompileCommands().
//!
//! Clean-slate implementation, loosely inspired by the-argus/zig-compile-commands.
//! All state lives on the `CompileCommands` struct, so instances can be
//! created and tested in isolation. The process wide instance that backs the
//! public API is instantiated in the parent module (./compile_commands.zig).
//!
//! zig 0.17 removed user defined `makeFn` steps: the whole build graph is
//! serialized into a configuration and executed by a separate maker process.
//! The generator therefore runs during the configure phase (walking the graph
//! is pure) and hands the resulting file to an `UpdateSourceFiles` step, which
//! is the declarative way to write generated content into the source tree.
//!
//! NOTE: leaking memory is fine here, the build graph is short lived and the
//! build system allocator is meant to be used this way.

const std = @import("std");
const db_mod = @import("./CompileCommandsDb.zig");

const CompileCommand = db_mod.CompileCommand;
const CompileCommandJson = db_mod.CompileCommandJson;
const CompileCommandsDb = db_mod.CompileCommandsDb;

pub const CompileCommands = struct {
    /// 100 build targets ought to be enough for everyone :)
    targets: [100]?*std.Build.Step.Compile = @splat(null),
    /// compiler driver written into every compile command entry
    driver: []const u8 = "clang++",

    /// Mark a compile step so its sources and flags end up in the generated
    /// compile_commands.json.
    pub fn addCompileCommands(self: *CompileCommands, target: *std.Build.Step.Compile) void {
        var i: usize = 0;
        while (i < self.targets.len) : (i += 1) {
            if (self.targets[i] == null) {
                self.targets[i] = target;
                return;
            }
        }
        @panic("CompileCommands.targets is full, cannot mark target");
    }

    /// Register the "compile-commands" build step.
    ///
    /// NOTE: must be called after all targets have been created and had their
    /// include paths / source files configured, otherwise they will be missed.
    pub fn addCompileCommandsStep(self: *CompileCommands, b: *std.Build) void {
        const json = generateCompileCommands(self, b) catch |err| {
            std.log.warn("compile-commands: generation failed: {}", .{err});
            return;
        };

        const update = b.addUpdateSourceFiles();
        update.addBytesToSource(json, "compile_commands.json");

        const top_level_step = b.step("compile-commands", "Build compile_commands.json file from marked targets");
        top_level_step.dependOn(&update.step);
    }
};

//
// OUTPUT
//

fn generateCompileCommands(state: *CompileCommands, b: *std.Build) ![]const u8 {
    const alloc = b.allocator;

    var db = CompileCommandsDb.init(alloc);
    var visited: std.AutoHashMapUnmanaged(*std.Build.Step.Compile, void) = .empty;

    var gather = Gather{ .db = &db };
    for (state.targets) |maybe_target| {
        const target = maybe_target orelse continue;
        try walkStep(&target.step, &gather, &visited);
    }

    // deterministic ordering so the output is stable across runs
    var entries: std.ArrayListUnmanaged(*CompileCommand) = .empty;
    var db_it = db.compile_commands_by_file_name.valueIterator();
    while (db_it.next()) |cc| try entries.append(alloc, cc.*);
    std.mem.sort(*CompileCommand, entries.items, {}, CompileCommand.lessThan);

    // The `output` field is cosmetic for clangd; generated artifacts live in
    // the cache (not resolvable at configure time), so point it at the build
    // root instead of the previous cache directory.
    const output_root = try b.root.joinString(alloc, ".");

    var json_writer = std.Io.Writer.Allocating.init(alloc);
    defer json_writer.deinit();

    var stringify = std.json.Stringify{
        .writer = &json_writer.writer,
        .options = .{ .whitespace = .indent_2 },
    };
    try stringify.beginArray();
    for (entries.items) |cc| {
        try stringify.write(try cc.toJson(alloc, output_root, state.driver));
    }
    try stringify.endArray();

    var list = json_writer.toArrayList();
    return list.toOwnedSlice(alloc);
}

//
// TRAVERSAL
//

const Gather = struct {
    db: *CompileCommandsDb,
};

/// Walks the build graph starting from all marked targets, visiting every
/// compile step that is reachable either through generic step dependencies or
/// by being linked into another compile step.
fn walkStep(
    step: *std.Build.Step,
    gather: *Gather,
    visited: *std.AutoHashMapUnmanaged(*std.Build.Step.Compile, void),
) anyerror!void {
    if (step.cast(std.Build.Step.Compile)) |compile_step| {
        try walkCompileStep(compile_step, gather, visited);
        return;
    }
    for (step.dependencies.items) |dependency| {
        try walkStep(dependency, gather, visited);
    }
}

fn walkCompileStep(
    step: *std.Build.Step.Compile,
    gather: *Gather,
    visited: *std.AutoHashMapUnmanaged(*std.Build.Step.Compile, void),
) anyerror!void {
    const gop = try visited.getOrPut(step.step.owner.allocator, step);
    if (gop.found_existing) return;

    const graph_modules = step.root_module.getGraph().modules;

    try gatherCompileStep(step, gather);

    for (step.step.dependencies.items) |dependency| {
        try walkStep(dependency, gather, visited);
    }
    for (graph_modules) |mod| for (mod.link_objects.items) |link_object| switch (link_object) {
        .other_step => |other| try walkCompileStep(other, gather, visited),
        else => {},
    };
}

//
// GATHERING
//

fn gatherCompileStep(step: *std.Build.Step.Compile, gather: *Gather) !void {
    const b = step.step.owner;
    const alloc = b.allocator;
    const graph_modules = step.root_module.getGraph().modules;

    var shared: std.ArrayListUnmanaged([]const u8) = .empty;

    for (graph_modules) |mod| {
        try appendModuleFlags(b, mod, &shared);
        for (mod.include_dirs.items) |include_dir| {
            try appendIncludeDir(b, include_dir, &shared);
        }
        for (mod.link_objects.items) |link_object| switch (link_object) {
            .system_lib => |system_lib| {
                try appendSystemLibFlags(b, system_lib, &shared);
            },
            else => {},
        };
    }

    // zig 0.17 dropped the global `--sysroot` build option; sysroots are now
    // modeled through the resolved target/query instead.
    // `Build.search_prefixes` was removed in zig 0.17 (it lives on the graph
    // now and observing it poisons the configure cache); the nix shell reaches
    // the same headers through CPATH, mirrored by appendEnvIncludes below.

    if (step.pie) |pie| try shared.append(alloc, if (pie) "-fPIE" else "-fno-PIE");
    if (step.lto) |lto| switch (lto) {
        .full => try shared.append(alloc, "-flto=full"),
        .thin => try shared.append(alloc, "-flto=thin"),
        .none => try shared.append(alloc, "-fno-lto"),
    };
    if (step.link_function_sections) try shared.append(alloc, "-ffunction-sections");
    if (step.link_data_sections) try shared.append(alloc, "-fdata-sections");

    // The public compile relevant surface of linked compile steps flows into
    // this step's flags, so its sources can resolve the dependency's headers.
    for (graph_modules) |mod| for (mod.link_objects.items) |link_object| switch (link_object) {
        .other_step => |other| try appendPublicFlags(b, other, &shared),
        else => {},
    };

    // The compiler honours CPATH, but it never lands in a module's include
    // dirs; mirror it so clangd resolves the same headers the build does.
    try appendEnvIncludes(b, &shared);

    for (graph_modules) |mod| for (mod.link_objects.items) |link_object| switch (link_object) {
        .c_source_file => |csf| {
            const file_abs = (try lazyPathAbsolute(b, csf.file)) orelse continue;
            var all_flags: std.ArrayListUnmanaged([]const u8) = .empty;
            try all_flags.appendSlice(alloc, shared.items);
            try all_flags.appendSlice(alloc, csf.flags);
            try gather.db.update(file_abs, all_flags.items);
        },
        .c_source_files => |csfs| {
            const root_abs = (try lazyPathAbsolute(b, csfs.root)) orelse continue;
            for (csfs.files) |file| {
                const file_abs = try std.fs.path.resolve(alloc, &.{ root_abs, file });
                var all_flags: std.ArrayListUnmanaged([]const u8) = .empty;
                try all_flags.appendSlice(alloc, shared.items);
                try all_flags.appendSlice(alloc, csfs.flags);
                try gather.db.update(file_abs, all_flags.items);
            }
        },
        else => {},
    };
}

/// Module level flags that matter to clangd, mirroring what zig passes to
/// clang when it compiles C/C++ sources of this module.
fn appendModuleFlags(
    b: *std.Build,
    mod: *std.Build.Module,
    out: *std.ArrayListUnmanaged([]const u8),
) !void {
    if (mod.stack_protector) |sp| try out.append(b.allocator, if (sp) "-fstack-protector" else "-fno-stack-protector");
    if (mod.omit_frame_pointer) |ofp| try out.append(b.allocator, if (ofp) "-fomit-frame-pointer" else "-fno-omit-frame-pointer");
    if (mod.sanitize_thread) |st| try out.append(b.allocator, if (st) "-fsanitize=thread" else "-fno-sanitize=thread");
    if (mod.pic) |pic| try out.append(b.allocator, if (pic) "-fPIC" else "-fno-PIC");
    if (mod.no_builtin) |nb| try out.append(b.allocator, if (nb) "-fno-builtin" else "-fbuiltin");

    if (mod.sanitize_c) |sc| switch (sc) {
        .off => try out.append(b.allocator, "-fno-sanitize=undefined"),
        .trap => try out.append(b.allocator, "-fsanitize-trap=undefined"),
        .full => try out.append(b.allocator, "-fsanitize=undefined"),
    };

    if (mod.dwarf_format) |dwarf_format| switch (dwarf_format) {
        .@"32" => try out.append(b.allocator, "-gdwarf32"),
        .@"64" => try out.append(b.allocator, "-gdwarf64"),
    };

    if (mod.optimize) |optimize| switch (optimize) {
        .Debug => try out.append(b.allocator, "-O0"),
        .ReleaseSmall => try out.append(b.allocator, "-Os"),
        .ReleaseFast, .ReleaseSafe => try out.append(b.allocator, "-O3"),
    };

    if (mod.code_model != .default) {
        try out.append(b.allocator, try std.fmt.allocPrint(b.allocator, "-mcmodel={s}", .{@tagName(mod.code_model)}));
    }

    try out.appendSlice(b.allocator, mod.c_macros.items);
}

/// Modified version of std.Build.Module.IncludeDir.appendZigProcessFlags,
/// mapping zig include directives to their clang equivalents.
fn appendIncludeDir(
    b: *std.Build,
    include_dir: std.Build.Module.IncludeDir,
    out: *std.ArrayListUnmanaged([]const u8),
) !void {
    switch (include_dir) {
        .path => |lp| try appendIncludeFlag(b, "-I", lp, out),
        .path_system => |lp| try appendIncludeFlag(b, "-isystem", lp, out),
        .path_after => |lp| try appendIncludeFlag(b, "-idirafter", lp, out),
        // generated include trees only exist in the cache, which is not known
        // during the configure phase (see the note at the top of this file)
        .config_header_step, .other_step => {},
        .framework_path, .framework_path_system, .embed_path => {},
    }
}

fn appendIncludeFlag(
    b: *std.Build,
    flag: []const u8,
    lazy_path: std.Build.LazyPath,
    out: *std.ArrayListUnmanaged([]const u8),
) !void {
    const path = (try lazyPathAbsolute(b, lazy_path)) orelse return;
    try out.append(b.allocator, flag);
    try out.append(b.allocator, path);
}

/// The compiler honours the CPATH environment variable for include search, but
/// it is not part of any module's include_dirs, so clangd would otherwise miss
/// those headers. Mirror it into the generated entry flags.
fn appendEnvIncludes(b: *std.Build, out: *std.ArrayListUnmanaged([]const u8)) !void {
    const cpath = b.graph.environ_map.get("CPATH") orelse return;
    var it = std.mem.tokenizeScalar(u8, cpath, ':');
    while (it.next()) |dir| {
        // CPATH has -I semantics; emitted after the module include dirs so
        // vendored headers keep precedence.
        try out.append(b.allocator, "-I");
        try out.append(b.allocator, dir);
    }
}

/// Append the compile relevant flags of a linked compile step (include dirs,
/// c macros, system libs). Its c sources get their own entries when the walk
/// reaches that step.
fn appendPublicFlags(
    b: *std.Build,
    dep: *std.Build.Step.Compile,
    out: *std.ArrayListUnmanaged([]const u8),
) !void {
    for (dep.root_module.getGraph().modules) |mod| {
        try out.appendSlice(b.allocator, mod.c_macros.items);
        for (mod.include_dirs.items) |include_dir| {
            try appendIncludeDir(b, include_dir, out);
        }
        for (mod.link_objects.items) |link_object| switch (link_object) {
            .system_lib => |system_lib| {
                try appendSystemLibFlags(b, system_lib, out);
            },
            else => {},
        };
    }
}

/// Emit the plain link flag of a system library. Include paths of system
/// libraries come in through CPATH (nix dev shell) and are mirrored by
/// appendEnvIncludes; pkg-config probing is not available at configure time in
/// zig 0.17, so there is nothing else to resolve here.
fn appendSystemLibFlags(
    b: *std.Build,
    system_lib: std.Build.Module.SystemLib,
    out: *std.ArrayListUnmanaged([]const u8),
) !void {
    try out.append(b.allocator, try std.fmt.allocPrint(b.allocator, "-l{s}", .{system_lib.name}));
}

/// Resolve a LazyPath to an absolute path during the configure phase, or null
/// when its location is only known at make time (generated files, cache
/// relative paths).
fn lazyPathAbsolute(
    b: *std.Build,
    lazy_path: std.Build.LazyPath,
) !?[]const u8 {
    const alloc = b.allocator;
    const path: std.Build.Cache.Path = switch (lazy_path) {
        .src_path => |sp| try sp.owner.root.join(alloc, sp.sub_path),
        .dependency => |dep| try dep.dependency.builder.root.join(alloc, dep.sub_path),
        .cwd_relative => |p| .{ .root_dir = .cwd(), .sub_path = p },
        .relative => |rel| switch (rel.base) {
            .cwd => .{ .root_dir = .cwd(), .sub_path = rel.sub_path },
            .build_root => .{ .root_dir = .cwd(), .sub_path = try b.root.root_dir.join(alloc, &.{ b.root.sub_path, rel.sub_path }) },
            // cache / install / deployed / zig prefix paths: unknown here
            else => return null,
        },
        .generated => return null,
    };

    const as_string = try path.root_dir.join(alloc, &.{path.subPathOrDot()});
    if (std.fs.path.isAbsolute(as_string)) return as_string;
    return try std.fs.path.resolve(alloc, &.{ try configureCwd(b), as_string });
}

/// Working directory of the configure process (zig 0.17 removed
/// `Build.Graph.cache`); used to anchor relative paths.
fn configureCwd(b: *std.Build) ![]const u8 {
    return std.process.currentPathAlloc(b.graph.io, b.allocator);
}
