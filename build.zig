const std = @import("std");

const Exts = []const []const u8;
const JUST_CPP: Exts = &[_][]const u8{".cpp"};
const JUST_C: Exts = &[_][]const u8{".c"};
const JUST_CC: Exts = &[_][]const u8{".cc"};
const JUST_H: Exts = &[_][]const u8{".h"};

// Ported over builds
// - deps:
// -- utf8cpp
// -- g3dlite
// -- libmpq
// -- detour
// -- fmt
//
// - src:
// -- common
// -- apps:
// -- -- authserver ?

// deps
var g3d_lite_lib: *std.Build.Step.Compile = undefined;
var detour_mod_lib: *std.Build.Step.Compile = undefined;
var recast_mod_lib: *std.Build.Step.Compile = undefined;
var fmt_mod_lib: *std.Build.Step.Compile = undefined;

// src
var common_mod_lib: *std.Build.Step.Compile = undefined;

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const options: CommonBuildOptions = .{
        .target = target,
        .optimize = optimize,
    };

    buildDeps(b, options) catch {
        @panic("Failed to buildDeps");
    };

    buildCommonLibrary(b, options) catch {
        @panic("Failed to buildCommonLibrary");
    };


    // buildAuthServer(b, options) catch {
    //     @panic("Failed to buildAuthServer");
    // };
}

const CommonBuildOptions = struct {
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
};

fn buildCommonLibrary(b: *std.Build, opts: CommonBuildOptions) !void {
    const gpa = b.allocator;
    const target = opts.target;
    const optimize = opts.optimize;

    const common_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libcpp = true,
    });

    linkG3DLiteLib(b, common_mod);
    linkDetour(b, common_mod);
    linkFmt(b, common_mod);

    common_mod.addIncludePath(b.path("./src/common"));

    const common_subdirs = try getAllFolders(gpa, "./src/common");
    for (common_subdirs.items) |dir| {
        common_mod.addIncludePath(b.path(dir));
    }

    const common_srcs = try getAllSources(gpa, "./src/common", JUST_CPP);
    for (common_srcs.items) |src| {
        common_mod.addCSourceFile(.{
            .file = b.path(src),
            .flags = &.{
                "-std=c++20",
            },
        });
    }

    common_mod_lib = b.addLibrary(.{
        .name = "common",
        .linkage = .static,
        .root_module = common_mod,
    });

    b.installArtifact(common_mod_lib);
}

fn linkCommon(b: *std.Build, target: *std.Build.Module) void {
    target.linkLibrary(common_mod_lib);
    for (common_mod_lib.root_module.include_dirs.items) |incl| {
        switch (incl) {
            .path => |p| {
                // std.log.info("Linking {s}", .{p.getDisplayName()});
                // TODO: filter out inner dependenecy include paths
                target.addIncludePath(p.dupe(b));
            },
            else => {},
        }
    }
}

fn buildAuthServer(b: *std.Build, opts: CommonBuildOptions) !void {
    const gpa = b.allocator;
    const target = opts.target;
    const optimize = opts.optimize;

    const authserver_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libcpp = true,
    });

    authserver_mod.addIncludePath(b.path("./src/server/apps/authserver"));
    authserver_mod.addIncludePath(b.path("./src/server/apps/authserver/Authentication"));
    authserver_mod.addIncludePath(b.path("./src/server/apps/authserver/Server"));

    linkCommon(b, authserver_mod);

    const authserver_cpp_srcs = try getAllSources(gpa, "./src/server/apps/authserver", JUST_CPP);
    for (authserver_cpp_srcs.items) |src| {
        authserver_mod.addCSourceFile(.{
            .file = b.path(src),
            .flags = &.{
                "-std=c++20",
            },
        });
    }

    const authserver_config_header = std.Build.Step.ConfigHeader.create(b, .{});
    authserver_mod.addConfigHeader(authserver_config_header);

    const authserver_exe = b.addExecutable(.{
        .name = "authserver",
        .root_module = authserver_mod,
    });

    b.installArtifact(authserver_exe);
}

fn buildDeps(b: *std.Build, opts: CommonBuildOptions) !void {
    const gpa = b.allocator;
    const target = opts.target;
    const optimize = opts.optimize;

    // G3D Lite library (see: deps/g3dlite/CMakeLists.txt)
    const g3d_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libcpp = true,
    });
    g3d_mod.addIncludePath(b.path("./deps/g3dlite/include"));

    const g3d_source_list = getPrefixedCppSrcs(gpa, "./deps/g3dlite/", g3d_lite_srcs) catch unreachable;
    for (g3d_source_list.items) |cpp_file_path| {
        g3d_mod.addCSourceFile(.{
            .file = b.path(cpp_file_path),
            .flags = &.{"-std=c++20"},
        });
    }
    g3d_mod.linkSystemLibrary("z", .{});

    // libMPQ (see: deps/libmpq/CMakeLists.txt)
    const libMPQ_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    libMPQ_mod.addIncludePath(b.path("./deps/libmpq/libmpq"));

    const libMPQ_srcs = getAllSources(gpa, "./deps/libmpq/libmpq", JUST_C) catch unreachable;
    for (libMPQ_srcs.items) |src| {
        libMPQ_mod.addCSourceFile(
            .{
                .file = b.path(src),
                .flags = &.{"-std=c89"},
            },
        );
    }
    libMPQ_mod.linkSystemLibrary("z", .{});
    libMPQ_mod.linkSystemLibrary("bzip2", .{});

    const libMPQ_config_header = std.Build.Step.ConfigHeader.create(b, .{});
    libMPQ_config_header.addValue("HAVE_LIBZ", i64, 1);
    libMPQ_config_header.addValue("PACKAGE", []const u8, "libmpq");
    libMPQ_config_header.addValue("PACKAGE_VERSION", []const u8, "0.4.2");
    libMPQ_config_header.addValue("VERSION", []const u8, "0.4.2");
    libMPQ_mod.addConfigHeader(libMPQ_config_header);

    // utf8cpp (see: deps/utf8cpp/CMakeLists.txt)
    const utf8cpp_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libcpp = true,
    });

    utf8cpp_mod.addIncludePath(b.path("./deps/utf8cpp"));
    //    utf8cpp_mod.addIncludePath(b.path("./deps/utf8cpp/utf8"));
    var utf8_srcs = getAllSources(gpa, "./deps/utf8cpp/utf8", JUST_H) catch unreachable;
    filterOutFile(&utf8_srcs, "cpp17.h");
    for (utf8_srcs.items) |src| {
        utf8cpp_mod.addCSourceFile(
            .{
                .file = b.path(src),
                .flags = &.{"-std=c++11"},
                .language = .cpp,
            },
        );
    }

    // builds
    g3d_lite_lib = b.addLibrary(.{
        .name = "g3d",
        .root_module = g3d_mod,
    });

    const libMPQ_lib = b.addLibrary(.{
        .name = "mpq",
        .root_module = libMPQ_mod,
    });

    const utf8cpp_lib = b.addLibrary(.{
        .name = "utf8cpp",
        .root_module = utf8cpp_mod,
    });

    b.installArtifact(g3d_lite_lib);
    b.installArtifact(libMPQ_lib);
    b.installArtifact(utf8cpp_lib);

    try buildDetour(b, opts);
    try buildFmt(b, opts);
}

const g3d_lite_srcs: []const []const u8 = &.{
    "source/AABox.cpp",
    "source/Any.cpp",
    "source/AnyTableReader.cpp",
    "source/BinaryFormat.cpp",
    "source/BinaryInput.cpp",
    "source/BinaryOutput.cpp",
    "source/Box.cpp",
    "source/Capsule.cpp",
    "source/CollisionDetection.cpp",
    "source/CoordinateFrame.cpp",
    "source/Crypto.cpp",
    "source/Cylinder.cpp",
    "source/debugAssert.cpp",
    "source/FileSystem.cpp",
    "source/fileutils.cpp",
    "source/format.cpp",
    "source/g3dfnmatch.cpp",
    "source/g3dmath.cpp",
    "source/GThread.cpp",
    "source/Line.cpp",
    "source/LineSegment.cpp",
    "source/Log.cpp",
    "source/Matrix3.cpp",
    "source/Matrix4.cpp",
    "source/MemoryManager.cpp",
    "source/PhysicsFrame.cpp",
    "source/Plane.cpp",
    "source/prompt.cpp",
    "source/Quat.cpp",
    "source/Random.cpp",
    "source/Ray.cpp",
    "source/RegistryUtil.cpp",
    "source/Sphere.cpp",
    "source/stringutils.cpp",
    "source/System.cpp",
    "source/TextInput.cpp",
    "source/TextOutput.cpp",
    "source/Triangle.cpp",
    "source/uint128.cpp",
    "source/UprightFrame.cpp",
    "source/Vector2.cpp",
    "source/Vector3.cpp",
    "source/Vector4.cpp",
};

fn getPrefixedCppSrcs(
    gpa: std.mem.Allocator,
    prefix: []const u8,
    srcs: []const []const u8,
) !std.ArrayListUnmanaged([]const u8) {
    var out = try std.ArrayListUnmanaged([]const u8).initCapacity(gpa, srcs.len);
    for (srcs) |src| {
        const joined = try std.mem.concat(gpa, u8, &.{ prefix, src });
        // std.log.debug("Adding to sourceset: '{s}'", .{joined});
        try out.append(gpa, joined);
    }
    return out;
}

// retrieve all directories from the provided 'dir' directory
fn getAllFolders(gpa: std.mem.Allocator, dir: []const u8) !std.ArrayListUnmanaged([]const u8) {
    var out = try std.ArrayListUnmanaged([]const u8).initCapacity(gpa, 32);
    const target_dir = try std.fs.cwd().openDir(dir, .{ .iterate = true });
    try scanFoldersRecursive(gpa, target_dir, &out);
    return out;
}

fn scanFoldersRecursive(
    gpa: std.mem.Allocator,
    curr: std.fs.Dir,
    output: *std.ArrayListUnmanaged([]const u8),
) !void {
    const root = try std.process.getCwdAlloc(gpa);
    defer gpa.free(root);
    var it = curr.iterate();
    while (try it.next()) |entry| {
        if (entry.kind == .directory) {
            const folder_path = try curr.realpathAlloc(gpa, entry.name);
            defer gpa.free(folder_path);
            const found_folder_path = try std.fs.path.relative(gpa, root, folder_path);

            try output.append(gpa, found_folder_path);

            // Recursively scan subdirectories
            const child_dir = try curr.openDir(entry.name, .{ .iterate = true });
            try scanFoldersRecursive(gpa, child_dir, output);
        }
    }
}

fn getAllSources(
    gpa: std.mem.Allocator,
    dir: []const u8,
    comptime extensions: Exts,
) !std.ArrayListUnmanaged([]const u8) {
    var out = try std.ArrayListUnmanaged([]const u8).initCapacity(gpa, 128);
    const target_dir = try std.fs.cwd().openDir(dir, .{ .iterate = true });
    try scanCppRecursive(gpa, target_dir, &out, extensions);
    return out;
}

fn scanCppRecursive(
    gpa: std.mem.Allocator,
    curr: std.fs.Dir,
    output: *std.ArrayListUnmanaged([]const u8),
    comptime extensions: Exts,
) !void {
    const root = try std.process.getCwdAlloc(gpa);
    defer gpa.free(root);

    var it = curr.iterate();
    while (try it.next()) |entry| {
        if (entry.kind == .directory) {
            const child_dir = try curr.openDir(entry.name, .{ .iterate = true });
            try scanCppRecursive(gpa, child_dir, output, extensions);
            return;
        }

        inline for (extensions) |ext| {
            if (std.mem.endsWith(u8, entry.name, ext)) {
                const cpp_file_path = try curr.realpathAlloc(gpa, entry.name);
                defer gpa.free(cpp_file_path);
                const found_cpp_file_path = try std.fs.path.relative(gpa, root, cpp_file_path);

                // std.log.debug("Adding cpp file = '{s}' to source set", .{found_cpp_file_path});
                try output.append(gpa, found_cpp_file_path);
            }
        }
    }
}

fn filterOutFile(srcs: *std.ArrayListUnmanaged([]const u8), file_name: []const u8) void {
    for (srcs.items, 0..) |src, i| {
        if (std.mem.endsWith(u8, src, file_name)) {
            _ = srcs.swapRemove(i);
            return;
        }
    }
}

fn buildDetour(b: *std.Build, options: CommonBuildOptions) !void {
    const gpa = b.allocator;

    const recast_detour_mod = b.createModule(.{
        .target = options.target,
        .optimize = options.optimize,
        .link_libcpp = true,
    });

    recast_detour_mod.addIncludePath(b.path("./deps/recastnavigation/Detour/Include"));

    const srcs = try getAllSources(gpa, "./deps/recastnavigation/Detour/Source", JUST_CPP);

    for (srcs.items) |src| {
        recast_detour_mod.addCSourceFile(.{
            .file = b.path(src),
            .flags = &.{"-std=c++14"},
        });
    }

    detour_mod_lib = b.addLibrary(.{
        .name = "detour",
        .root_module = recast_detour_mod,
    });

    b.installArtifact(detour_mod_lib);
}

fn linkDetour(b: *std.Build, target: *std.Build.Module) void {
    target.linkLibrary(detour_mod_lib);
    for (detour_mod_lib.root_module.include_dirs.items) |incl| {
        target.addIncludePath(incl.path.dupe(b));
    }
}

fn buildRecast(b: *std.Build, options: CommonBuildOptions) !void {
    const gpa = b.allocator;

    const recast_mod = b.createModule(.{
        .target = options.target,
        .optimize = options.optimize,
        .link_libcpp = true,
    });

    recast_mod.addIncludePath(b.path("./deps/recastnavigation/Recast/Include"));

    const srcs = try getAllSources(gpa, "./deps/recastnavigation/Recast/Source", JUST_CPP);

    for (srcs.items) |src| {
        recast_mod.addCSourceFile(.{
            .file = b.path(src),
            .flags = &.{"-std=c++14"},
        });
    }

    recast_mod_lib = b.addLibrary(.{
        .name = "recast",
        .root_module = recast_mod,
    });

    b.installArtifact(recast_mod_lib);
}

fn buildFmt(b: *std.Build, options: CommonBuildOptions) !void {
    const gpa = b.allocator;

    const fmt_mod = b.createModule(.{
        .target = options.target,
        .optimize = options.optimize,
        .link_libcpp = true,
    });

    fmt_mod.addIncludePath(b.path("./deps/fmt/include"));
    fmt_mod.addCMacro("FMT_CONSTEVAL", " ");

    const srcs = try getAllSources(gpa, "./deps/fmt/src/", JUST_CC);

    for (srcs.items) |src| {
        // std.log.info("Linking {s}", .{src});
        fmt_mod.addCSourceFile(.{
            .file = b.path(src),
            .flags = &.{"-std=c++20"},
        });
    }

    fmt_mod_lib = b.addLibrary(.{
        .name = "fmtlib",
        .root_module = fmt_mod,
    });

    b.installArtifact(fmt_mod_lib);
}

fn linkFmt(b: *std.Build, target: *std.Build.Module) void {
    target.linkLibrary(fmt_mod_lib);
    for (fmt_mod_lib.root_module.include_dirs.items) |incl| {
        target.addIncludePath(incl.path.dupe(b));
    }
    target.addCMacro("FMT_CONSTEVAL", " ");
}

fn linkRecast(b: *std.Build, target: *std.Build.Module) void {
    target.linkLibrary(recast_mod_lib);
    for (recast_mod_lib.root_module.include_dirs.items) |incl| {
        target.addIncludePath(incl.path.dupe(b));
    }
}

fn linkG3DLiteLib(b: *std.Build, target: *std.Build.Module) void {
    target.addIncludePath(b.path("./deps/g3dlite/include"));
    target.linkLibrary(g3d_lite_lib);
}
