const std = @import("std");
const utils = @import("./zig-build/build_utils.zig");
const deps = @import("./zig-build/deps.zig");
const common = @import("./zig-build/common.zig");
const database = @import("./zig-build/database.zig");

// src
var common_mod_lib: *std.Build.Step.Compile = undefined;
var database_mod_lib: *std.Build.Step.Compile = undefined;
var shared_mod_lib: *std.Build.Step.Compile = undefined;

pub fn build(b: *std.Build) void {
    const env_map = std.process.getEnvMap(b.allocator) catch {
        @panic("Failed to read env");
    };

    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const boost_path = env_map.get("BOOST_PATH") orelse {
        @panic("no boost path");
    };

    const options: utils.CommonBuildOptions = .{
        .target = target,
        .optimize = optimize,
        .boost_path = boost_path,
    };

    deps.buildDeps(b, options) catch {
        @panic("FAIL: buildDeps");
    };

    common.buildCommon(b, options) catch {
        @panic("FAIL: buildCommon");
    };

    database.buildDatabase(b, options) catch {
        @panic("FAIL: buildDatabase");
    };
}
