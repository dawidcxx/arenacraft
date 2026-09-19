//! compile_commands.json generation from compile steps marked with
//! addCompileCommands().
//!
//! Clean-slate implementation, loosely inspired by the-argus/zig-compile-commands.
//! All state lives on the `CompileCommands` struct, so instances can be
//! created and tested in isolation. The process wide instance that backs the
//! public API is instantiated in the parent module (./compile_commands.zig).
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
        const cc_step = CompileCommandsStep.create(b, self);

        // Wire dependencies on every LazyPath that generation resolves at make
        // time (generated config headers, emitted include trees, generated
        // sources), so all of them exist by the time the step runs.
        self.wireLazyPathDependencies(&cc_step.step);

        const top_level_step = b.step("compile-commands", "Build compile_commands.json file from marked targets");
        top_level_step.dependOn(&cc_step.step);
    }

    fn wireLazyPathDependencies(self: *CompileCommands, cc_step: *std.Build.Step) void {
        var visited: std.AutoHashMapUnmanaged(*std.Build.Step.Compile, void) = .empty;
        for (self.targets) |maybe_target| {
            const target = maybe_target orelse continue;
            walkStep(&target.step, .{ .wiring = cc_step }, &visited) catch |err| {
                std.log.warn("compile-commands: failed to wire lazy path dependencies: {}", .{err});
            };
        }
    }
};

/// Heap allocated so the make function can recover the owning state through
/// the step pointer via @fieldParentPtr.
const CompileCommandsStep = struct {
    step: std.Build.Step,
    state: *CompileCommands,

    fn create(b: *std.Build, state: *CompileCommands) *CompileCommandsStep {
        const self = b.allocator.create(CompileCommandsStep) catch {
            @panic("OOM");
        };
        self.* = .{
            .step = std.Build.Step.init(.{
                .id = .custom,
                .name = "compile-commands/step",
                .owner = b,
                .makeFn = make,
            }),
            .state = state,
        };
        return self;
    }

    fn make(step: *std.Build.Step, make_options: std.Build.Step.MakeOptions) anyerror!void {
        _ = make_options;
        const self: *CompileCommandsStep = @fieldParentPtr("step", step);
        try generateCompileCommands(self.state, step.owner);
    }
};

//
// OUTPUT
//

fn generateCompileCommands(state: *CompileCommands, b: *std.Build) !void {
    const alloc = b.allocator;

    var db = CompileCommandsDb.init(alloc);
    var pkg_config_cache: std.StringHashMapUnmanaged([]const []const u8) = .empty;
    var visited: std.AutoHashMapUnmanaged(*std.Build.Step.Compile, void) = .empty;

    var gather = Gather{ .db = &db, .pkg_config_cache = &pkg_config_cache };
    for (state.targets) |maybe_target| {
        const target = maybe_target orelse continue;
        try walkStep(&target.step, .{ .gather = &gather }, &visited);
    }

    // deterministic ordering so the output is stable across runs
    var entries: std.ArrayListUnmanaged(*CompileCommand) = .empty;
    var db_it = db.compile_commands_by_file_name.valueIterator();
    while (db_it.next()) |cc| try entries.append(alloc, cc.*);
    std.mem.sort(*CompileCommand, entries.items, {}, CompileCommand.lessThan);

    const output_root = b.graph.global_cache_root.path orelse (b.cache_root.path orelse "zig-cache");

    var json_writer = std.Io.Writer.Allocating.init(alloc);
    defer json_writer.deinit();

    var stringify = std.json.Stringify{
        .writer = &json_writer.writer,
        .options = .{ .emit_null_optional_fields = true, .whitespace = .indent_2 },
    };
    try stringify.beginArray();
    for (entries.items) |cc| {
        try stringify.write(try cc.toJson(alloc, output_root, state.driver));
    }
    try stringify.endArray();

    const json_string: []const u8 = try json_writer.toOwnedSlice();
    defer alloc.free(json_string);

    const compile_commands_file = try b.build_root.handle.createFile(b.graph.io, "compile_commands.json", .{});
    defer compile_commands_file.close(b.graph.io);
    try compile_commands_file.writeStreamingAll(b.graph.io, json_string);
}

//
// TRAVERSAL
//

const WalkMode = union(enum) {
    /// add step dependencies on every LazyPath that will be resolved later
    wiring: *std.Build.Step,
    /// resolve everything into compile command entries
    gather: *Gather,
};

const Gather = struct {
    db: *CompileCommandsDb,
    pkg_config_cache: *std.StringHashMapUnmanaged([]const []const u8),
};

/// Walks the build graph starting from all marked targets, visiting every
/// compile step that is reachable either through generic step dependencies or
/// by being linked into another compile step. Both walk modes traverse the
/// exact same set of steps on purpose, so the wiring phase is guaranteed to
/// have wired everything the gather phase later resolves.
fn walkStep(
    step: *std.Build.Step,
    mode: WalkMode,
    visited: *std.AutoHashMapUnmanaged(*std.Build.Step.Compile, void),
) anyerror!void {
    if (step.cast(std.Build.Step.Compile)) |compile_step| {
        try walkCompileStep(compile_step, mode, visited);
        return;
    }
    for (step.dependencies.items) |dependency| {
        try walkStep(dependency, mode, visited);
    }
}

fn walkCompileStep(
    step: *std.Build.Step.Compile,
    mode: WalkMode,
    visited: *std.AutoHashMapUnmanaged(*std.Build.Step.Compile, void),
) anyerror!void {
    const gop = try visited.getOrPut(step.step.owner.allocator, step);
    if (gop.found_existing) return;

    const graph_modules = step.root_module.getGraph().modules;

    switch (mode) {
        .wiring => |cc_step| {
            for (graph_modules) |mod| {
                for (mod.include_dirs.items) |include_dir| switch (include_dir) {
                    .path => |lp| lp.addStepDependencies(cc_step),
                    .path_system => |lp| lp.addStepDependencies(cc_step),
                    .path_after => |lp| lp.addStepDependencies(cc_step),
                    .config_header_step => |ch| ch.getOutputDir().addStepDependencies(cc_step),
                    .other_step => |comp| comp.getEmittedIncludeTree().addStepDependencies(cc_step),
                    .framework_path, .framework_path_system, .embed_path => {},
                };
                for (mod.link_objects.items) |link_object| switch (link_object) {
                    .c_source_file => |csf| csf.file.addStepDependencies(cc_step),
                    .c_source_files => |csfs| csfs.root.addStepDependencies(cc_step),
                    else => {},
                };
            }
        },
        .gather => |gather| try gatherCompileStep(step, gather),
    }

    for (step.step.dependencies.items) |dependency| {
        try walkStep(dependency, mode, visited);
    }
    for (graph_modules) |mod| for (mod.link_objects.items) |link_object| switch (link_object) {
        .other_step => |other| try walkCompileStep(other, mode, visited),
        else => {},
    };
}

//
// GATHERING
//

fn gatherCompileStep(step: *std.Build.Step.Compile, gather: *Gather) !void {
    const b = step.step.owner;
    const asking = &step.step;
    const alloc = b.allocator;
    const graph_modules = step.root_module.getGraph().modules;

    var shared: std.ArrayListUnmanaged([]const u8) = .empty;

    for (graph_modules) |mod| {
        try appendModuleFlags(b, mod, &shared);
        for (mod.include_dirs.items) |include_dir| {
            try appendIncludeDir(b, include_dir, asking, &shared, false);
        }
        for (mod.link_objects.items) |link_object| switch (link_object) {
            .system_lib => |system_lib| {
                try appendSystemLibFlags(b, asking, system_lib, gather.pkg_config_cache, &shared);
            },
            else => {},
        };
    }

    if (b.sysroot) |sysroot| {
        try shared.append(alloc, "--sysroot");
        try shared.append(alloc, sysroot);
    }
    for (b.search_prefixes.items) |search_prefix| {
        try shared.append(alloc, "-I");
        try shared.append(alloc, try std.fs.path.resolve(alloc, &.{ b.graph.cache.cwd, search_prefix, "include" }));
    }

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
        .other_step => |other| try appendPublicFlags(b, other, asking, gather.pkg_config_cache, &shared),
        else => {},
    };

    // The compiler honours CPATH, but it never lands in a module's include
    // dirs; mirror it so clangd resolves the same headers the build does.
    try appendEnvIncludes(b, &shared);

    for (graph_modules) |mod| for (mod.link_objects.items) |link_object| switch (link_object) {
        .c_source_file => |csf| {
            const file_abs = try lazyPathAbsolute(b, csf.file, asking);
            var all_flags: std.ArrayListUnmanaged([]const u8) = .empty;
            try all_flags.appendSlice(alloc, shared.items);
            try all_flags.appendSlice(alloc, csf.flags);
            try gather.db.update(file_abs, all_flags.items);
        },
        .c_source_files => |csfs| {
            const root_abs = try lazyPathAbsolute(b, csfs.root, asking);
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
    asking: *std.Build.Step,
    out: *std.ArrayListUnmanaged([]const u8),
    warn_on_skip: bool,
) !void {
    switch (include_dir) {
        .path => |lp| try appendIncludeFlag(b, "-I", lp, asking, out),
        .path_system => |lp| try appendIncludeFlag(b, "-isystem", lp, asking, out),
        .path_after => |lp| try appendIncludeFlag(b, "-idirafter", lp, asking, out),
        .config_header_step => |ch| try appendIncludeFlag(b, "-I", ch.getOutputDir(), asking, out),
        .other_step => |comp| try appendIncludeFlag(b, "-I", comp.getEmittedIncludeTree(), asking, out),
        .framework_path, .framework_path_system, .embed_path => {
            if (warn_on_skip) {
                std.log.warn("compile-commands: include directive '{s}' has no clangd equivalent, skipping", .{@tagName(include_dir)});
            }
        },
    }
}

fn appendIncludeFlag(
    b: *std.Build,
    flag: []const u8,
    lazy_path: std.Build.LazyPath,
    asking: *std.Build.Step,
    out: *std.ArrayListUnmanaged([]const u8),
) !void {
    try out.append(b.allocator, flag);
    try out.append(b.allocator, try lazyPathAbsolute(b, lazy_path, asking));
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
    asking: *std.Build.Step,
    pkg_config_cache: *std.StringHashMapUnmanaged([]const []const u8),
    out: *std.ArrayListUnmanaged([]const u8),
) !void {
    for (dep.root_module.getGraph().modules) |mod| {
        try out.appendSlice(b.allocator, mod.c_macros.items);
        for (mod.include_dirs.items) |include_dir| {
            try appendIncludeDir(b, include_dir, asking, out, false);
        }
        for (mod.link_objects.items) |link_object| switch (link_object) {
            .system_lib => |system_lib| {
                try appendSystemLibFlags(b, asking, system_lib, pkg_config_cache, out);
            },
            else => {},
        };
    }
}

/// Ask pkg-config for the real flags of a system library, falling back to a
/// plain -l flag when it is unavailable. This is what pulls in the actual
/// include paths of nix provided libraries.
fn appendSystemLibFlags(
    b: *std.Build,
    asking: *std.Build.Step,
    system_lib: std.Build.Module.SystemLib,
    cache: *std.StringHashMapUnmanaged([]const []const u8),
    out: *std.ArrayListUnmanaged([]const u8),
) !void {
    const alloc = b.allocator;

    if (cache.get(system_lib.name)) |cached_flags| {
        try out.appendSlice(alloc, cached_flags);
        return;
    }

    switch (system_lib.use_pkg_config) {
        .no => try out.append(alloc, try std.fmt.allocPrint(alloc, "-l{s}", .{system_lib.name})),
        .yes, .force => {
            if (std.Build.Step.Compile.runPkgConfig(asking, system_lib.name)) |result| {
                var all_flags: std.ArrayListUnmanaged([]const u8) = .empty;
                // split "-I<dir>" into two tokens so they deduplicate against
                // the "-I" + path pairs emitted for module include dirs
                for (result.cflags) |cflag| {
                    if (std.mem.startsWith(u8, cflag, "-I") and cflag.len > "-I".len) {
                        try all_flags.append(alloc, "-I");
                        try all_flags.append(alloc, cflag["-I".len..]);
                    } else {
                        try all_flags.append(alloc, cflag);
                    }
                }
                try all_flags.appendSlice(alloc, result.libs);
                const flags_slice = try all_flags.toOwnedSlice(alloc);
                try cache.put(alloc, try alloc.dupe(u8, system_lib.name), flags_slice);
                try out.appendSlice(alloc, flags_slice);
            } else |err| switch (err) {
                error.PkgConfigInvalidOutput,
                error.PkgConfigCrashed,
                error.PkgConfigFailed,
                error.PkgConfigNotInstalled,
                error.PackageNotFound,
                => {
                    std.log.warn("compile-commands: pkg-config failed for '{s}' ({}), falling back to -l flag", .{ system_lib.name, err });
                    try out.append(alloc, try std.fmt.allocPrint(alloc, "-l{s}", .{system_lib.name}));
                },
                else => |e| return e,
            }
        },
    }
}

fn lazyPathAbsolute(
    b: *std.Build,
    lazy_path: std.Build.LazyPath,
    asking: *std.Build.Step,
) ![]const u8 {
    const as_string = try lazy_path.getPath3(b, asking).toString(b.allocator);
    if (std.fs.path.isAbsolute(as_string)) return as_string;
    return std.fs.path.resolve(b.allocator, &.{ b.graph.cache.cwd, as_string });
}
