const std = @import("std");
const Build = std.Build;

const Deps = @import("./Deps.zig").Deps;
const cpp = @import("./cppkit-zig/build.zig");

pub const BuildRequest = struct { *Build, Build.ResolvedTarget, std.builtin.OptimizeMode };

/// The build graph of the ported core: third-party `deps` plus one member
/// per ported module (common, later database, shared, ...).
pub const AcGraph = struct {
    gpa: std.mem.Allocator,
    io: std.Io,

    deps: Deps = .{},

    common: struct {
        module: *Build.Module = undefined,
        library: *Build.Step.Compile = undefined,
    } = .{},

    const Self = @This();

    pub fn create(gpa: std.mem.Allocator, io: std.Io) *Self {
        var instance = gpa.create(Self) catch unreachable;
        instance.* = .{
            .gpa = gpa,
            .io = io,
        };
        instance.deps.back_reference = instance;
        return instance;
    }

    pub fn destroy(self: *Self) void {
        self.gpa.destroy(self);
    }

    pub fn build(self: *Self, b: *Build) !void {
        const target = b.standardTargetOptions(.{});
        const optimize = b.standardOptimizeOption(.{});
        const build_request: BuildRequest = .{ b, target, optimize };

        try self.buildDeps(build_request);
        try self.buildTestBinary(build_request);

        try self.buildCommon(build_request);
        try self.buildAc(build_request);
    }

    fn buildDeps(self: *Self, b: BuildRequest) !void {
        try self.deps.build(b);
    }

    /// The ported common library (src-port/common) - ours, not a dependency.
    fn buildCommon(self: *Self, b: BuildRequest) !void {
        const bl = b[0];
        const target = b[1];
        const optimize = b[2];

        const module = bl.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        // common uses flat-name includes across subdirectories, mirroring
        // the CMake CollectIncludeDirectories behavior
        try cpp.addFlatIncludes(bl, "src-port/common", module);

        // placeholder revision data, mirrors the old zig build; real values
        // need a git describe run step (see ZIG_BUILD_MIGRATION.md open questions)
        const revision_header = std.Build.Step.ConfigHeader.create(bl, .{});
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

        module.addCMacro("_CONF_DIR", "\"/todo\"");

        var sources = cpp.querySources(self.gpa, self.io, "src-port/common", .{
            .extensions = cpp.Exts.JUST_CPP,
            .recursive = true,
        });

        module.addCSourceFiles(.{
            .files = sources.get(),
            .language = .cpp,
            .flags = &.{"-std=c++23"},
        });

        const deps = &self.deps;
        deps.linkUtf8(module);
        deps.linkArgon2(module);
        deps.linkFmt(module);
        deps.linkBoost(module);
        deps.linkOpenSSL(module);
        deps.linkHiredis(module);
        deps.linkDetour(module);
        deps.linkG3DLite(module);

        const library = bl.addLibrary(.{
            .name = "common",
            .root_module = module,
            .linkage = .static,
        });

        // expose the headers to every consumer linking this library; note
        // that flat-name include resolution still needs the subdirectory
        // paths on the consumer (cpp.addFlatIncludes)
        library.installHeadersDirectory(bl.path("src-port/common"), "", .{});

        // update graph at the end
        self.common.module = module;
        self.common.library = library;

        const isolated = bl.step("common", "Build the ported common library in isolation");
        isolated.dependOn(&library.step);
    }

    fn buildTestBinary(self: *Self, b: BuildRequest) !void {
        const bl = b[0];
        const target = b[1];
        const optimize = b[2];

        const module = bl.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        module.addCSourceFile(.{
            .file = bl.path("zig-build/Test.cpp"),
            .language = .cpp,
            .flags = &.{"-std=c++20"},
        });

        const deps = &self.deps;
        deps.linkUtf8(module);
        deps.linkArgon2(module);
        deps.linkDetour(module);
        deps.linkFmt(module);
        deps.linkBoost(module);

        deps.linkOpenSSL(module);
        deps.linkHiredis(module);
        deps.linkMysqlClient(module);

        const exe = bl.addExecutable(.{
            .name = "test-build",
            .root_module = module,
        });

        // linked dependencies are picked up by the walk automatically
        cpp.addCompileCommands(exe);

        const install = bl.addInstallArtifact(exe, .{});
        const run = bl.addRunArtifact(exe);
        run.step.dependOn(&install.step);

        const step = bl.step("test-build", "Build and run the linking smoke test");
        step.dependOn(&run.step);
    }

    /// The `ac` master executable: an argparse router (src-port/main.cpp) that
    /// forwards arguments 1:1 to the ported sub programs (src-port/tools/*).
    fn buildAc(self: *Self, b: BuildRequest) !void {
        const bl = b[0];
        const target = b[1];
        const optimize = b[2];

        const alloc = self.gpa;
        const io = self.io;

        const module = bl.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        // src-port root (subcommands.h) + flat-name includes for common and
        // the tool sources
        module.addIncludePath(bl.path("src-port"));
        try cpp.addFlatIncludes(bl, "src-port/common", module);
        try cpp.addFlatIncludes(bl, "src-port/tools", module);

        module.addCSourceFiles(.{
            .root = bl.path("src-port"),
            .files = &.{"main.cpp"},
            .language = .cpp,
            .flags = &.{"-std=c++23"},
        });

        var tool_sources = cpp.querySources(alloc, io, "src-port/tools", .{
            .extensions = cpp.Exts.JUST_CPP,
            .recursive = true,
        });

        module.addCSourceFiles(.{
            .files = tool_sources.get(),
            .language = .cpp,
            .flags = &.{"-std=c++23"},
        });

        const deps = &self.deps;
        module.linkLibrary(self.common.library);
        deps.linkArgparse(module);
        deps.linkMpq(module);
        deps.linkRecast(module);
        deps.linkDetour(module);
        deps.linkG3DLite(module);
        deps.linkFmt(module);
        deps.linkBoost(module);
        deps.linkUtf8(module);
        deps.linkArgon2(module);
        deps.linkOpenSSL(module);

        const exe = bl.addExecutable(.{
            .name = "ac",
            .root_module = module,
        });

        const install = bl.addInstallArtifact(exe, .{});
        cpp.addCompileCommands(exe);

        const step = bl.step("ac", "Build the ac master executable");
        step.dependOn(&install.step);
    }
};

test "AcGraph compiles" {
    const t = std.testing;
    var graph = AcGraph.create(t.allocator, t.io);
    defer graph.destroy();
}
