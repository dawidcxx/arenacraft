const std = @import("std");
const Build = std.Build;

const Deps = @import("./Deps.zig").Deps;
const Src = @import("./Src.zig").Src;
const TestHarness = @import("./TestHarness.zig").TestHarness;
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
    tests: TestHarness = .{},

    const Self = @This();

    pub fn create(gpa: std.mem.Allocator, io: std.Io) *Self {
        var instance = gpa.create(Self) catch unreachable;
        instance.* = .{
            .gpa = gpa,
            .io = io,
        };
        instance.deps.back_reference = instance;
        instance.src.back_reference = instance;
        instance.tests.back_reference = instance;
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
        try self.buildUnitTests(build_request);
    }

    /// Unit tests, convention-over-configuration (see TestHarness.zig).
    /// `zig build test` builds and runs every enabled target's tests.
    fn buildUnitTests(self: *Self, b: BuildRequest) !void {
        const bl = b[0];

        const common_run = try self.tests.build(b, .common);
        const game_run = try self.tests.build(b, .game);

        const test_step = bl.step("test", "Build and run all unit tests");
        test_step.dependOn(common_run);
        test_step.dependOn(game_run);
    }

    fn buildDeps(self: *Self, b: BuildRequest) !void {
        try self.deps.build(b);
    }

    fn buildSrc(self: *Self, b: BuildRequest) !void {
        try self.src.build(b);
    }

    // Smoke test for our build system itself
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

    /// The `ac` master executable: an argparse router (src/ac/main.cpp) that
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

        // co-located with subcommands.h
        module.addIncludePath(bl.path("src/ac"));

        module.addCSourceFiles(.{
            .root = bl.path("src/ac"),
            .files = &.{"main.cpp"},
            .language = .cpp,
            .flags = &Src.core_cflags,
        });

        const deps = &self.deps;
        const src = &self.src;

        // linkWorldserver cascades: worldserver -> game -> shared -> database -> common
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

        // `zig build ac` - build only
        const step = bl.step("ac", "Build the ac master executable");
        step.dependOn(&install.step);

        // `zig build run-ac [-- <args>]` - build and run, forwarding args
        const run = bl.addRunArtifact(exe);
        run.step.dependOn(&install.step);
        if (bl.args) |args| run.addArgs(args);

        const run_step = bl.step("run-ac", "Build and run the ac master executable");
        run_step.dependOn(&run.step);
    }
};

test "AcGraph compiles" {
    const t = std.testing;
    var graph = AcGraph.create(t.allocator, t.io);
    defer graph.destroy();
}
