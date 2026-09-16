// Ported core modules ("our" sources from src-port/) - mirrors the Deps.zig
// pattern: one entry per module with its build fn, plus link helpers that
// encapsulate how consumers consume a module (library + flat-name includes).
//
// AcGraph holds an instance under `src`, next to `deps`.

const std = @import("std");
const Build = std.Build;

const cpp = @import("./cppkit-zig/build.zig");
const AcGraph = @import("./BuildCommons.zig").AcGraph;
const BuildRequest = @import("./BuildCommons.zig").BuildRequest;

pub const Src = struct {
    pub const core_cflags = [_][]const u8{
        "-std=c++23",
        // zig 0.16 clang defaults:
        // - fmt 11's operator"" _a fallback shim is deprecation-errored
        // - newer asio deprecates basic_deadline_timer/null_buffers, which the
        //   AC DeadlineTimer/Socket wrappers inherit/use
        "-Wno-deprecated-literal-operator",
        "-Wno-deprecated-declarations",
    };

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

    back_reference: *AcGraph = undefined,

    const Self = @This();

    pub fn build(self: *Self, b: BuildRequest) !void {
        try self.buildCommon(b);
        try self.buildDatabase(b);
        try self.buildShared(b);
        try self.buildAuth(b);
        try self.buildTools(b);
    }

    //
    // link helpers - how consumer modules consume a ported module.
    // they cascade (CMake PUBLIC propagation parity): linking shared gives
    // you database and common includes too.
    //

    pub fn linkCommon(self: *Self, b: *Build, mod: *Build.Module) !void {
        mod.linkLibrary(self.common.library);
        // flat-name include resolution ("MapDefines.h" etc)
        try cpp.addFlatIncludes(b, "src-port/common", mod);
    }

    pub fn linkDatabase(self: *Self, b: *Build, mod: *Build.Module) !void {
        mod.linkLibrary(self.database.library);
        try cpp.addFlatIncludes(b, "src-port/database", mod);
        try self.linkCommon(b, mod);
    }

    pub fn linkShared(self: *Self, b: *Build, mod: *Build.Module) !void {
        mod.linkLibrary(self.shared.library);
        try cpp.addFlatIncludes(b, "src-port/shared", mod);
        try self.linkDatabase(b, mod);
    }

    pub fn linkAuth(self: *Self, b: *Build, mod: *Build.Module) !void {
        mod.linkLibrary(self.auth.library);
        try cpp.addFlatIncludes(b, "src-port/auth", mod);
        try self.linkShared(b, mod);
    }

    pub fn linkTools(self: *Self, b: *Build, mod: *Build.Module) !void {
        mod.linkLibrary(self.tools.library);
        try cpp.addFlatIncludes(b, "src-port/tools", mod);
        try self.linkCommon(b, mod);
    }

    //
    // builds
    //

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
        try cpp.addFlatIncludes(b, "src-port/common", module);

        // placeholder revision data, mirrors the old zig build; real values
        // need a git describe run step (see ZIG_BUILD_MIGRATION.md open questions)
        const revision_header = std.Build.Step.ConfigHeader.create(b, .{});
        revision_header.include_path = "revision.h";
        revision_header.addValue("_HASH", []const u8, "");
        revision_header.addValue("_DATE", []const u8, "");
        revision_header.addValue("_BRANCH", []const u8, "");
        revision_header.addValue("_CMAKE_COMMAND", []const u8, "zig build");
        revision_header.addValue("_CMAKE_VERSION", []const u8, "0.16.0");
        revision_header.addValue("_CMAKE_HOST_SYSTEM", []const u8, "nix");
        revision_header.addValue("_BUILD_DIRECTORY", []const u8, "");
        revision_header.addValue("_SOURCE_DIRECTORY", []const u8, "");
        revision_header.addValue("_MYSQL_EXECUTABLE", []const u8, "");
        module.addConfigHeader(revision_header);

        var sources = cpp.querySources(self.back_reference.gpa, self.back_reference.io, "src-port/common", .{
            .extensions = cpp.Exts.JUST_CPP,
            .recursive = true,
        });

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
        deps.linkOpenSSL(module);
        deps.linkHiredis(module);
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
        library.installHeadersDirectory(b.path("src-port/common"), "", .{});

        // update graph at the end
        self.common.module = module;
        self.common.library = library;

        const isolated = b.step("common", "Build the ported common library in isolation");
        isolated.dependOn(&library.step);
    }

    /// DB migration machinery (Updater/) is intentionally not ported - see
    /// ZIG_BUILD_MIGRATION.md decisions.
    fn buildDatabase(self: *Self, build_request: BuildRequest) !void {
        const b = build_request[0];
        const target = build_request[1];
        const optimize = build_request[2];

        const module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        try cpp.addFlatIncludes(b, "src-port/database", module);

        var sources = cpp.querySources(self.back_reference.gpa, self.back_reference.io, "src-port/database", .{
            .extensions = cpp.Exts.JUST_CPP,
            .recursive = true,
        });

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
        deps.linkMysqlClient(module);

        const library = b.addLibrary(.{
            .name = "database",
            .root_module = module,
            .linkage = .static,
        });

        library.installHeadersDirectory(b.path("src-port/database"), "", .{});

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

        try cpp.addFlatIncludes(b, "src-port/shared", module);

        var sources = cpp.querySources(self.back_reference.gpa, self.back_reference.io, "src-port/shared", .{
            .extensions = cpp.Exts.JUST_CPP,
            .recursive = true,
        });

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

        library.installHeadersDirectory(b.path("src-port/shared"), "", .{});

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

        try cpp.addFlatIncludes(b, "src-port/auth", module);

        var sources = cpp.querySources(self.back_reference.gpa, self.back_reference.io, "src-port/auth", .{
            .extensions = cpp.Exts.JUST_CPP,
            .recursive = true,
        });

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
        deps.linkOpenSSL(module);
        deps.linkHiredis(module);
        deps.linkArgparse(module);
        deps.linkMysqlClient(module);

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

        try cpp.addFlatIncludes(b, "src-port/tools", module);

        var sources = cpp.querySources(self.back_reference.gpa, self.back_reference.io, "src-port/tools", .{
            .extensions = cpp.Exts.JUST_CPP,
            .recursive = true,
        });

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
        deps.linkOpenSSL(module);

        // update graph at the end
        self.tools.module = module;
        self.tools.library = b.addLibrary(.{
            .name = "tools",
            .root_module = module,
            .linkage = .static,
        });
    }
};
