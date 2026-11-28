// Ported core modules ("our" sources from src/) - mirrors the Deps.zig
// pattern: one entry per module with its build fn, plus link helpers that
// encapsulate how consumers consume a module (library + flat-name includes).
//
// AcGraph holds an instance under `src`, next to `deps`.

const std = @import("std");
const Build = std.Build;

const cpp = @import("./cppkit-zig/build.zig");
const AcGraph = @import("./BuildCommons.zig").AcGraph;
const BuildRequest = @import("./BuildCommons.zig").BuildRequest;
const BuildCommons = @import("./BuildCommons.zig");

const unknown_revision = "Unknown";

fn runGit(b: *Build, args: []const []const u8) ?[]const u8 {
    const root = b.root.joinString(b.allocator, ".") catch @panic("OOM");

    var argv: std.ArrayListUnmanaged([]const u8) = .empty;
    defer argv.deinit(b.allocator);
    argv.appendSlice(b.allocator, &.{ "git", "-C", root }) catch @panic("OOM");
    argv.appendSlice(b.allocator, args) catch @panic("OOM");

    const result = std.process.run(b.allocator, b.graph.io, .{
        .argv = argv.items,
        .cwd = .{ .path = root },
        .environ_map = &b.graph.environ_map,
        .stdout_limit = .limited(64 * 1024),
        .stderr_limit = .limited(64 * 1024),
    }) catch return null;
    defer b.allocator.free(result.stdout);
    defer b.allocator.free(result.stderr);

    const exit_code = switch (result.term) {
        .exited => |code| code,
        else => return null,
    };
    if (exit_code != 0) return null;

    const trimmed = std.mem.trim(u8, result.stdout, " \t\r\n");
    if (trimmed.len == 0) return null;
    return b.dupe(trimmed);
}

/// Reads a revision value from a `-D` override first (those are part of zig's
/// configure cache key), then the build environment (Docker's legacy path,
/// which has to poison the configure cache), then git in the source checkout.
/// Missing or broken values become "Unknown" rather than failing the build.
const RevisionValue = struct {
    value: []const u8,
    from_env: bool,
};

fn revisionValue(
    b: *Build,
    option_name: []const u8,
    option_description: []const u8,
    env_names: []const []const u8,
    git_args: []const []const u8,
) RevisionValue {
    if (b.option([]const u8, option_name, option_description)) |value| {
        const trimmed = std.mem.trim(u8, value, " \t\r\n");
        if (trimmed.len != 0) return .{ .value = b.dupe(trimmed), .from_env = false };
    }

    for (env_names) |name| {
        if (b.graph.environ_map.get(name)) |value| {
            const trimmed = std.mem.trim(u8, value, " \t\r\n");
            if (trimmed.len != 0) return .{ .value = b.dupe(trimmed), .from_env = true };
        }
    }

    return .{ .value = runGit(b, git_args) orelse unknown_revision, .from_env = false };
}

/// Register the git files the revision is derived from as configure
/// dependencies (`-Dpure-configure`). HEAD, the loose ref it points at, the
/// parent ref directories (entry sets) and packed-refs cover every
/// commit/checkout/pack operation that can change `git rev-parse HEAD`'s
/// output. Missing paths cannot be tracked (zig fails configure when a
/// registered path does not exist), so they are skipped - deleting a tracked
/// path still forces a reconfigure, and the parent directory entry set catches
/// a packed ref turning loose again. Any git failure falls back to poisoning
/// the cache, so an unusual layout can never serve a stale revision.
fn trackGitRevision(b: *Build) void {
    const root = b.root.joinString(b.allocator, ".") catch @panic("OOM");

    const out = runGit(b, &.{
        "rev-parse",
        "--absolute-git-dir",
        "--git-common-dir",
        "--git-path",
        "HEAD",
        "--git-path",
        "packed-refs",
    }) orelse return b.graph.poisonCache();

    var lines = std.mem.splitScalar(u8, out, '\n');
    const git_dir = lines.next() orelse return b.graph.poisonCache();
    const common_dir = lines.next() orelse return b.graph.poisonCache();
    const head = lines.next() orelse return b.graph.poisonCache();
    const packed_refs = lines.next() orelse return b.graph.poisonCache();
    if (git_dir.len == 0 or common_dir.len == 0 or head.len == 0) return b.graph.poisonCache();

    const common_abs = resolvePath(b, root, common_dir);
    trackExistingFile(b, resolvePath(b, root, head));
    trackExistingFile(b, resolvePath(b, root, packed_refs));

    const ref = runGit(b, &.{ "symbolic-ref", "-q", "HEAD" }) orelse return;
    if (!std.mem.startsWith(u8, ref, "refs/")) return;

    const loose_ref = std.fs.path.join(b.allocator, &.{ common_abs, ref }) catch @panic("OOM");
    trackExistingFile(b, loose_ref);

    var dir: ?[]const u8 = std.fs.path.dirname(loose_ref);
    while (dir) |d| : (dir = std.fs.path.dirname(d)) {
        if (d.len <= common_abs.len) break;
        trackExistingDir(b, d);
    }
}

fn resolvePath(b: *Build, root: []const u8, path: []const u8) []const u8 {
    if (path.len == 0 or std.fs.path.isAbsolute(path)) return path;
    return std.fs.path.join(b.allocator, &.{ root, path }) catch @panic("OOM");
}

fn trackExistingFile(b: *Build, abs_path: []const u8) void {
    const stat = std.Io.Dir.cwd().statFile(b.graph.io, abs_path, .{}) catch return;
    if (stat.kind != .file) return;
    b.dependOnFileContents(b.graph.cwdRelativePath(abs_path));
}

fn trackExistingDir(b: *Build, abs_path: []const u8) void {
    const stat = std.Io.Dir.cwd().statFile(b.graph.io, abs_path, .{}) catch return;
    // a ref path component may itself be a file (e.g. a ref named like a dir)
    if (stat.kind != .directory) return;
    b.dependOnDirectoryMetadata(b.graph.cwdRelativePath(abs_path));
}

pub const Src = struct {
    pub const core_cflags = [_][]const u8{
        "-std=c++23",
        // zig 0.17 clang defaults:
        // - fmt 11's operator"" _a fallback shim is deprecation-errored
        // - newer asio deprecates basic_deadline_timer/null_buffers, which the
        //   AC DeadlineTimer/Socket wrappers inherit/use
        "-Wno-deprecated-literal-operator",
        "-Wno-deprecated-declarations",
        // keep frame pointers so the crash handler's backtrace() can walk the
        // stack even with optimized code
        "-fno-omit-frame-pointer",
        BuildCommons.no_ubsan,
    };

    revision: struct {
        module: *Build.Module = undefined,
        library: *Build.Step.Compile = undefined,
    } = .{},

    common: struct {
        module: *Build.Module = undefined,
        library: *Build.Step.Compile = undefined,
    } = .{},

    database: struct {
        module: *Build.Module = undefined,
        library: *Build.Step.Compile = undefined,
    } = .{},

    shared: struct {
        module: *Build.Module = undefined,
        library: *Build.Step.Compile = undefined,
    } = .{},

    auth: struct {
        module: *Build.Module = undefined,
        library: *Build.Step.Compile = undefined,
    } = .{},

    tools: struct {
        module: *Build.Module = undefined,
        library: *Build.Step.Compile = undefined,
    } = .{},

    game: struct {
        module: *Build.Module = undefined,
        library: *Build.Step.Compile = undefined,
    } = .{},

    worldserver: struct {
        module: *Build.Module = undefined,
        library: *Build.Step.Compile = undefined,
    } = .{},

    back_reference: *AcGraph = undefined,

    const Self = @This();

    /// Query a module's `.cpp` sources, excluding unit-test translation units:
    /// the `Tests.cpp` entrypoint and co-located `*_test.cpp` files belong to
    /// the test harness (TestHarness.zig), never to the module library.
    fn queryModuleSources(self: *Self, b: *Build, dir: []const u8) cpp.SourceSet {
        var sources = cpp.querySources(self.back_reference.gpa, self.back_reference.io, dir, .{
            .extensions = cpp.Exts.JUST_CPP,
            .recursive = true,
            .build = b,
        });
        sources.filterOutByGlob("*/Tests.cpp");
        sources.filterOutByGlob("*_test.cpp");
        return sources;
    }

    pub fn build(self: *Self, b: BuildRequest) !void {
        try self.buildRevision(b);
        try self.buildCommon(b);
        try self.buildDatabase(b);
        try self.buildShared(b);
        try self.buildAuth(b);
        try self.buildTools(b);
        try self.buildGame(b);
        try self.buildWorldserver(b);
    }

    //
    // link helpers - how consumer modules consume a ported module.
    // they cascade (CMake PUBLIC propagation parity): linking shared gives
    // you database and common includes too.
    //

    /// Revision data changes on every commit, so it lives in its own library:
    /// `common` must not depend on it or every push would invalidate `common`.
    /// Final executables link it for the GitRevision symbols common references.
    pub fn linkRevision(self: *Self, b: *Build, mod: *Build.Module) !void {
        _ = b;
        mod.linkLibrary(self.revision.library);
    }

    pub fn linkCommon(self: *Self, b: *Build, mod: *Build.Module) !void {
        mod.linkLibrary(self.common.library);
        // flat-name include resolution ("MapDefines.h" etc)
        try cpp.addFlatIncludes(b, "src/common", mod);
    }

    pub fn linkDatabase(self: *Self, b: *Build, mod: *Build.Module) !void {
        mod.linkLibrary(self.database.library);
        try cpp.addFlatIncludes(b, "src/database", mod);
        try self.linkCommon(b, mod);
    }

    pub fn linkShared(self: *Self, b: *Build, mod: *Build.Module) !void {
        mod.linkLibrary(self.shared.library);
        try cpp.addFlatIncludes(b, "src/shared", mod);
        try self.linkDatabase(b, mod);
    }

    pub fn linkAuth(self: *Self, b: *Build, mod: *Build.Module) !void {
        mod.linkLibrary(self.auth.library);
        try cpp.addFlatIncludes(b, "src/auth", mod);
        try self.linkShared(b, mod);
    }

    pub fn linkTools(self: *Self, b: *Build, mod: *Build.Module) !void {
        mod.linkLibrary(self.tools.library);
        try cpp.addFlatIncludes(b, "src/tools", mod);
        try self.linkCommon(b, mod);
    }

    pub fn linkGame(self: *Self, b: *Build, mod: *Build.Module) !void {
        mod.linkLibrary(self.game.library);
        try cpp.addFlatIncludes(b, "src/game", mod);
        try self.linkShared(b, mod);
    }

    pub fn linkWorldserver(self: *Self, b: *Build, mod: *Build.Module) !void {
        mod.linkLibrary(self.worldserver.library);
        try cpp.addFlatIncludes(b, "src/worldserver", mod);
        try self.linkGame(b, mod);
    }

    //
    // builds
    //

    /// `GitRevision.cpp` is the only translation unit that consumes the
    /// generated `revision.h`. Keeping it out of `common` means a new
    /// GIT_COMMIT/GIT_DATE only rebuilds this one file plus the final link,
    /// instead of invalidating the whole `common` (and every module linking it).
    fn buildRevision(self: *Self, build_request: BuildRequest) !void {
        const b = build_request[0];
        const target = build_request[1];
        const optimize = build_request[2];

        const module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        try cpp.addFlatIncludes(b, "src/common", module);

        const git_hash = revisionValue(
            b,
            "git-commit",
            "Commit hash for revision.h (overrides GIT_* env)",
            &.{ "GIT_COMMIT", "ARENACRAFT_GIT_COMMIT" },
            &.{ "rev-parse", "HEAD" },
        );
        const git_branch = revisionValue(
            b,
            "git-branch",
            "Branch name for revision.h (overrides GIT_* env)",
            &.{ "GIT_BRANCH", "ARENACRAFT_GIT_BRANCH" },
            &.{ "rev-parse", "--abbrev-ref", "HEAD" },
        );
        const git_date = revisionValue(
            b,
            "git-date",
            "Commit date for revision.h (overrides GIT_* env)",
            &.{ "GIT_DATE", "ARENACRAFT_GIT_DATE" },
            &.{ "show", "-s", "--format=%cI", "HEAD" },
        );

        if (git_hash.from_env or git_branch.from_env or git_date.from_env) {
            // Environment variables are not part of zig's configure cache key.
            // Poisoning keeps these values out of the cache, but it cannot
            // reject a configuration cached by an earlier build (that hit
            // happens before this code runs); anything outside git should pass
            // -Dgit-commit/-Dgit-branch/-Dgit-date, which are part of the key.
            if (self.back_reference.pure_configure) {
                std.log.warn(
                    "-Dpure-configure with GIT_* revision env is unsafe; pass -Dgit-commit/-Dgit-branch/-Dgit-date",
                    .{},
                );
            }
            b.graph.poisonCache();
        } else if (self.back_reference.pure_configure) {
            trackGitRevision(b);
        }

        const revision_header = std.Build.Step.ConfigHeader.create(b, .{});
        revision_header.include_path = "revision.h";
        revision_header.addValue("_HASH", []const u8, git_hash.value);
        revision_header.addValue("_DATE", []const u8, git_date.value);
        revision_header.addValue("_BRANCH", []const u8, git_branch.value);
        revision_header.addValue("_CMAKE_COMMAND", []const u8, "zig build");
        revision_header.addValue("_CMAKE_VERSION", []const u8, "0.16.0");
        revision_header.addValue("_CMAKE_HOST_SYSTEM", []const u8, "nix");
        revision_header.addValue("_BUILD_DIRECTORY", []const u8, "");
        revision_header.addValue("_SOURCE_DIRECTORY", []const u8, "");
        revision_header.addValue("_MYSQL_EXECUTABLE", []const u8, "");
        module.addConfigHeader(revision_header);

        module.addCSourceFile(.{
            .file = b.path("src/common/GitRevision.cpp"),
            .language = .cpp,
            .flags = &core_cflags,
        });

        const library = b.addLibrary(.{
            .name = "revision",
            .root_module = module,
            .linkage = .static,
        });

        self.revision.module = module;
        self.revision.library = library;
    }

    fn buildCommon(self: *Self, build_request: BuildRequest) !void {
        const b = build_request[0];
        const target = build_request[1];
        const optimize = build_request[2];

        const module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        // common uses flat-name includes across subdirectories, mirroring
        // the CMake CollectIncludeDirectories behavior
        try cpp.addFlatIncludes(b, "src/common", module);

        var sources = self.queryModuleSources(b, "src/common");
        sources.filterOut("GitRevision.cpp");

        module.addCSourceFiles(.{
            .files = sources.get(),
            .language = .cpp,
            .flags = &core_cflags,
        });

        const deps = &self.back_reference.deps;
        deps.linkUtf8(module);
        deps.linkArgon2(module);
        deps.linkFmt(module);
        deps.linkBoost(module);
        deps.linkDetour(module);
        deps.linkG3DLite(module);

        const library = b.addLibrary(.{
            .name = "common",
            .root_module = module,
            .linkage = .static,
        });

        // expose the headers to every consumer linking this library; note
        // that flat-name include resolution still needs the subdirectory
        // paths on the consumer (linkCommon)
        library.installHeadersDirectory(b.path("src/common"), "", cpp.header_install_options);

        // update graph at the end
        self.common.module = module;
        self.common.library = library;

        const isolated = b.step("common", "Build the ported common library in isolation");
        isolated.dependOn(&library.step);
    }

    /// Database library; Updater/ holds the native `ac db-update` implementation.
    fn buildDatabase(self: *Self, build_request: BuildRequest) !void {
        const b = build_request[0];
        const target = build_request[1];
        const optimize = build_request[2];

        const module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        try cpp.addFlatIncludes(b, "src/database", module);

        var sources = self.queryModuleSources(b, "src/database");

        module.addCSourceFiles(.{
            .files = sources.get(),
            .language = .cpp,
            .flags = &core_cflags,
        });

        try self.linkCommon(b, module);

        const deps = &self.back_reference.deps;
        // common's headers pull in boost/fmt, so the database module needs
        // their include paths + defines too (no transitive propagation)
        deps.linkFmt(module);
        deps.linkBoost(module);

        const library = b.addLibrary(.{
            .name = "database",
            .root_module = module,
            .linkage = .static,
        });

        library.installHeadersDirectory(b.path("src/database"), "", cpp.header_install_options);

        // update graph at the end
        self.database.module = module;
        self.database.library = library;

        const isolated = b.step("database", "Build the ported database library in isolation");
        isolated.dependOn(&library.step);
    }

    fn buildShared(self: *Self, build_request: BuildRequest) !void {
        const b = build_request[0];
        const target = build_request[1];
        const optimize = build_request[2];

        const module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        try cpp.addFlatIncludes(b, "src/shared", module);

        var sources = self.queryModuleSources(b, "src/shared");

        module.addCSourceFiles(.{
            .files = sources.get(),
            .language = .cpp,
            .flags = &core_cflags,
        });

        try self.linkDatabase(b, module);

        const deps = &self.back_reference.deps;
        // shared headers pull in boost/fmt via common (no transitive propagation)
        deps.linkFmt(module);
        deps.linkBoost(module);
        deps.linkUtf8(module);

        const library = b.addLibrary(.{
            .name = "shared",
            .root_module = module,
            .linkage = .static,
        });

        library.installHeadersDirectory(b.path("src/shared"), "", cpp.header_install_options);

        // update graph at the end
        self.shared.module = module;
        self.shared.library = library;

        const isolated = b.step("shared", "Build the ported shared library in isolation");
        isolated.dependOn(&library.step);
    }

    fn buildAuth(self: *Self, build_request: BuildRequest) !void {
        const b = build_request[0];
        const target = build_request[1];
        const optimize = build_request[2];

        const module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        try cpp.addFlatIncludes(b, "src/auth", module);

        var sources = self.queryModuleSources(b, "src/auth");

        module.addCSourceFiles(.{
            .files = sources.get(),
            .language = .cpp,
            .flags = &core_cflags,
        });

        try self.linkShared(b, module);

        const deps = &self.back_reference.deps;
        // auth headers pull in fmt/boost via common (no transitive propagation)
        deps.linkFmt(module);
        deps.linkBoost(module);
        deps.linkUtf8(module);
        deps.linkArgparse(module);

        // update graph at the end
        self.auth.module = module;
        self.auth.library = b.addLibrary(.{
            .name = "auth",
            .root_module = module,
            .linkage = .static,
        });
    }

    fn buildTools(self: *Self, build_request: BuildRequest) !void {
        const b = build_request[0];
        const target = build_request[1];
        const optimize = build_request[2];

        const module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        try cpp.addFlatIncludes(b, "src/tools", module);

        var sources = self.queryModuleSources(b, "src/tools");

        module.addCSourceFiles(.{
            .files = sources.get(),
            .language = .cpp,
            .flags = &core_cflags,
        });

        try self.linkCommon(b, module);

        const deps = &self.back_reference.deps;
        deps.linkMpq(module);
        deps.linkRecast(module);
        deps.linkDetour(module);
        deps.linkG3DLite(module);
        deps.linkFmt(module);
        deps.linkBoost(module);
        deps.linkUtf8(module);
        deps.linkArgon2(module);

        // update graph at the end
        self.tools.module = module;
        self.tools.library = b.addLibrary(.{
            .name = "tools",
            .root_module = module,
            .linkage = .static,
        });
    }

    fn buildGame(self: *Self, build_request: BuildRequest) !void {
        const b = build_request[0];
        const target = build_request[1];
        const optimize = build_request[2];

        const module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        try cpp.addFlatIncludes(b, "src/game", module);

        var sources = self.queryModuleSources(b, "src/game");

        module.addCSourceFiles(.{
            .files = sources.get(),
            .language = .cpp,
            .flags = &core_cflags,
        });

        try self.linkShared(b, module);

        const deps = &self.back_reference.deps;
        deps.linkFmt(module);
        deps.linkBoost(module);
        deps.linkUtf8(module);
        // game headers include <G3D/...> and <DetourNavMesh.h> (via common)
        deps.linkG3DLite(module);
        deps.linkDetour(module);

        const library = b.addLibrary(.{
            .name = "game",
            .root_module = module,
            .linkage = .static,
        });

        library.installHeadersDirectory(b.path("src/game"), "", cpp.header_install_options);

        // update graph at the end
        self.game.module = module;
        self.game.library = library;

        const isolated = b.step("game", "Build the ported game library in isolation");
        isolated.dependOn(&library.step);
    }

    fn buildWorldserver(self: *Self, build_request: BuildRequest) !void {
        const b = build_request[0];
        const target = build_request[1];
        const optimize = build_request[2];

        const module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        try cpp.addFlatIncludes(b, "src/worldserver", module);

        var sources = self.queryModuleSources(b, "src/worldserver");

        module.addCSourceFiles(.{
            .files = sources.get(),
            .language = .cpp,
            .flags = &core_cflags,
        });

        try self.linkGame(b, module);

        const deps = &self.back_reference.deps;
        deps.linkGsoap(module);
        deps.linkFmt(module);
        deps.linkBoost(module);
        deps.linkUtf8(module);
        deps.linkArgparse(module);
        deps.linkG3DLite(module);
        deps.linkDetour(module);

        // update graph at the end
        self.worldserver.module = module;
        self.worldserver.library = b.addLibrary(.{
            .name = "worldserver",
            .root_module = module,
            .linkage = .static,
        });
    }
};
