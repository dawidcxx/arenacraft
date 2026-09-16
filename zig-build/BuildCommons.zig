const std = @import("std");
const Build = std.Build;

const Deps = @import("./Deps.zig").Deps;

pub const BuildRequest = struct { *Build, Build.ResolvedTarget, std.builtin.OptimizeMode };

pub const AcGraph = struct {
    gpa: std.mem.Allocator,
    io: std.Io,

    deps: Deps = .{},

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
    }

    fn buildDeps(self: *Self, b: BuildRequest) !void {
        try self.deps.build(b);
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

        const install = bl.addInstallArtifact(exe, .{});
        const run = bl.addRunArtifact(exe);
        run.step.dependOn(&install.step);

        const step = bl.step("test-build", "Build and run the linking smoke test");
        step.dependOn(&run.step);
    }
};

test "AcGraph compiles" {
    const t = std.testing;
    var graph = AcGraph.create(t.allocator, t.io);
    defer graph.destroy();
}
