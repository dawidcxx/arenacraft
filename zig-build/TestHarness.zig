//! Convention-over-configuration unit test harness (doctest).
//!
//! A module opts into unit testing by providing an entrypoint at
//! `src/<module>/Tests.cpp` - the only translation unit that defines
//! `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`. Individual test cases live in
//! co-located `*_test.cpp` files anywhere under the module and are discovered
//! automatically (no manual source lists).
//!
//! For every enabled target the harness registers two steps:
//!   <module>-test-build   build and install the test executable
//!   run-<module>-test     build and run it (args after `--` are forwarded)
//!
//! doctest is header-only and comes from the system (nix `doctest`, on CPATH).

const std = @import("std");
const Build = std.Build;

const cpp = @import("./cppkit-zig/build.zig");
const AcGraph = @import("./BuildCommons.zig").AcGraph;
const BuildRequest = @import("./BuildCommons.zig").BuildRequest;
const Src = @import("./Src.zig").Src;

/// Modules that have a `Tests.cpp` entrypoint and can be unit tested.
pub const Target = enum {
    common,
    game,
};

pub const TestHarness = struct {
    back_reference: *AcGraph = undefined,

    const Self = @This();

    /// Build the test target and register its build/run steps.
    /// Returns the run step so callers can aggregate it.
    pub fn build(self: *Self, b: BuildRequest, target: Target) !*Build.Step {
        const bl = b[0];
        const resolved_target = b[1];
        const optimize = b[2];

        const graph = self.back_reference;
        const name = @tagName(target);
        const src_dir = try std.fs.path.join(graph.gpa, &.{ "src", name });
        const entry = try std.fs.path.join(graph.gpa, &.{ src_dir, "Tests.cpp" });

        const module = bl.createModule(.{
            .target = resolved_target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        // doctest main lives in the module's Tests.cpp entrypoint
        module.addCSourceFile(.{
            .file = bl.path(entry),
            .language = .cpp,
            .flags = &Src.core_cflags,
        });

        // convention: every co-located `*_test.cpp` is a test translation unit
        var tests = cpp.querySources(graph.gpa, graph.io, src_dir, .{
            .extensions = cpp.Exts.JUST_CPP,
            .recursive = true,
        });
        tests.filterToGlob("*_test.cpp");
        module.addCSourceFiles(.{
            .files = tests.get(),
            .language = .cpp,
            .flags = &Src.core_cflags,
        });

        try self.linkTarget(bl, module, target);

        const exe = bl.addExecutable(.{
            .name = try std.fmt.allocPrint(graph.gpa, "{s}-tests", .{name}),
            .root_module = module,
        });

        const install = bl.addInstallArtifact(exe, .{});
        cpp.addCompileCommands(exe);

        // `zig build <module>-test-build` - build and install the executable
        const build_step = bl.step(
            try std.fmt.allocPrint(graph.gpa, "{s}-test-build", .{name}),
            try std.fmt.allocPrint(graph.gpa, "Build the {s} unit test executable", .{name}),
        );
        build_step.dependOn(&install.step);

        // `zig build run-<module>-test [-- <doctest args>]`
        const run = bl.addRunArtifact(exe);
        run.step.dependOn(&install.step);
        if (bl.args) |args| run.addArgs(args);

        const run_step = bl.step(
            try std.fmt.allocPrint(graph.gpa, "run-{s}-test", .{name}),
            try std.fmt.allocPrint(graph.gpa, "Build and run the {s} unit tests", .{name}),
        );
        run_step.dependOn(&run.step);

        return run_step;
    }

    /// Give the test module the same library, include and external dependency
    /// closure as the module under test, so tests have its normal access.
    fn linkTarget(self: *Self, b: *Build, mod: *Build.Module, target: Target) !void {
        const graph = self.back_reference;
        const src = &graph.src;
        const deps = &graph.deps;

        switch (target) {
            .common => {
                try src.linkCommon(b, mod);
                deps.linkUtf8(mod);
                deps.linkArgon2(mod);
                deps.linkFmt(mod);
                deps.linkBoost(mod);
                deps.linkDetour(mod);
                deps.linkG3DLite(mod);
            },
            .game => {
                try src.linkGame(b, mod);
                deps.linkFmt(mod);
                deps.linkBoost(mod);
                deps.linkUtf8(mod);
                deps.linkG3DLite(mod);
                deps.linkDetour(mod);
            },
        }

        // final executable: resolve the unvendored system libs (openssl, ...)
        deps.linkSystemLibraries(mod);
    }
};
