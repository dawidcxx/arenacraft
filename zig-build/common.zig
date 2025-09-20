const std = @import("std");
const utils = @import("./build_utils.zig");
const deps = @import("./deps.zig");

const CommonBuildOptions = utils.CommonBuildOptions;

var common_lib: *std.Build.Step.Compile = undefined;

pub fn buildCommon(b: *std.Build, opts: CommonBuildOptions) !void {
    try buildCommonLib(b, opts);
}

//
// Links
//
pub fn linkCommon(b: *std.Build, target: *std.Build.Module) void {
    target.linkLibrary(common_lib);
    for (common_lib.root_module.include_dirs.items) |incl| {
        switch (incl) {
            .path => |p| {
                target.addIncludePath(p.dupe(b));
            },
            else => {},
        }
    }
}

//
// Builds
//

fn buildCommonLib(b: *std.Build, options: CommonBuildOptions) !void {
    const mod = b.createModule(.{
        .target = options.target,
        .optimize = options.optimize,
        .link_libcpp = true,
    });

    deps.linkDetour(b, mod);
    deps.linkFmt(b, mod);
    deps.linkG3DLite(b, mod);
    deps.linkArgon2(b, mod);
    deps.linkBoost(b, mod);
    deps.linkUtf8(b, mod);
    deps.linkZstd(b, mod);

    mod.linkSystemLibrary("hiredis", .{ .needed = true, .preferred_link_mode = .static });
    mod.linkSystemLibrary("openssl", .{});
    mod.linkSystemLibrary("zlib", .{});

    mod.addIncludePath(b.path("./src/common"));
    const subdirs = try utils.getAllFolders(b.allocator, "./src/common");
    for (subdirs.items) |dir| {
        // std.log.info("Appending to include path '{s}'", .{dir});
        mod.addIncludePath(b.path(dir));
    }

    const srcs = try utils.getAllSources(b.allocator, "./src/common", utils.JUST_CPP);
    for (srcs.items) |src| {
        // std.log.info("Adding '{s}'", .{src});
        mod.addCSourceFile(.{
            .file = b.path(src),
            .flags = &.{"-std=c++23"},
        });
    }

    mod.addCMacro("_CONF_DIR", "\"/todo\"");

    {
        const revision_header = std.Build.Step.ConfigHeader.create(b, .{});
        revision_header.include_path = "revision.h";
        revision_header.addValue("_HASH", []const u8, "");
        revision_header.addValue("_DATE", []const u8, "");
        revision_header.addValue("_BRANCH", []const u8, "");
        revision_header.addValue("_CMAKE_COMMAND", []const u8, "zig build");
        revision_header.addValue("_CMAKE_VERSION", []const u8, "0.14");
        revision_header.addValue("_CMAKE_HOST_SYSTEM", []const u8, "nixos");
        revision_header.addValue("_BUILD_DIRECTORY", []const u8, "");
        revision_header.addValue("_SOURCE_DIRECTORY", []const u8, "");
        revision_header.addValue("_MYSQL_EXECUTABLE", []const u8, "");
        mod.addConfigHeader(revision_header);
    }

    common_lib = b.addLibrary(.{
        .name = "common",
        .root_module = mod,
    });

    b.installArtifact(common_lib);
}
