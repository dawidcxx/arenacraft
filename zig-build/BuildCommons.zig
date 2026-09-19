const std = @import("std");
const Build = std.Build;

const Deps = @import("./Deps.zig").Deps;
const Src = @import("./Src.zig").Src;
const cpp = @import("./cppkit-zig/build.zig");

// zig 0.16 ships a ubsan runtime that aborts in Debug; the legacy codebase
// (loadlib/mpq readers etc.) contains deliberate unaligned access patterns
pub const no_ubsan = "-fno-sanitize=undefined";

pub const BuildRequest = struct { *Build, Build.ResolvedTarget, std.builtin.OptimizeMode };

/// The build graph of the ported core: third-party `deps`, the ported
/// modules under `src` (see Src.zig), and the master executable targets.
pub const AcGraph = struct {
    gpa: std.mem.Allocator,
    io: std.Io,

    deps: Deps = .{},
    src: Src = .{},

    const Self = @This();

    pub fn create(gpa: std.mem.Allocator, io: std.Io) *Self {
        var instance = gpa.create(Self) catch unreachable;
        instance.* = .{
            .gpa = gpa,
            .io = io,
        };
        instance.deps.back_reference = instance;
        instance.src.back_reference = instance;
        return instance;
    }

    pub fn destroy(self: *Self) void {
        self.gpa.destroy(self);
    }

    pub fn build(self: *Self, b: *Build) !void {
        // Third-party headers arrive via CPATH, supplied by `nix develop` /
        // the Dockerfile builder stage. Fail fast with a useful message rather
        // than deep inside a translation unit with a bare "file not found".
        const env = &b.graph.environ_map;
        if (env.get("CPATH") == null) {
            std.log.err("missing build environment variable CPATH; run inside `nix develop` or the Dockerfile builder stage", .{});
            return error.MissingBuildEnvironment;
        }

        const target = b.standardTargetOptions(.{});
        const optimize = b.standardOptimizeOption(.{});
        const build_request: BuildRequest = .{ b, target, optimize };

        try self.buildDeps(build_request);
        try self.buildSrc(build_request);
        try self.buildTestBinary(build_request);
        try self.buildAc(build_request);
    }

    fn buildDeps(self: *Self, b: BuildRequest) !void {
        try self.deps.build(b);
    }

    fn buildSrc(self: *Self, b: BuildRequest) !void {
        try self.src.build(b);
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
            .flags = &.{ "-std=c++20", no_ubsan },
        });

        const deps = &self.deps;
        deps.linkUtf8(module);
        deps.linkArgon2(module);
        deps.linkDetour(module);
        deps.linkFmt(module);
        deps.linkBoost(module);

        deps.linkSystemLibraries(module);

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

    /// The `ac` master executable: an argparse router (src/main.cpp) that
    /// forwards arguments 1:1 to the ported sub programs. Sub programs live in
    /// the src modules (auth, tools); the router only needs their entry points.
    fn buildAc(self: *Self, b: BuildRequest) !void {
        const bl = b[0];
        const target = b[1];
        const optimize = b[2];

        const module = bl.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        // src root (subcommands.h)
        module.addIncludePath(bl.path("src"));

        module.addCSourceFiles(.{
            .root = bl.path("src"),
            .files = &.{"main.cpp"},
            .language = .cpp,
            .flags = &Src.core_cflags,
        });

        const deps = &self.deps;
        const src = &self.src;

        // linkWorldserver cascades: modules/scripts -> game -> shared -> database -> common
        try src.linkWorldserver(bl, module);
        try src.linkAuth(bl, module);
        try src.linkTools(bl, module);
        deps.linkArgparse(module);
        deps.linkSystemLibraries(module);

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
