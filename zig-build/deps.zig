const std = @import("std");
const utils = @import("./build_utils.zig");

const CommonBuildOptions = utils.CommonBuildOptions;

var g3d_lite_lib: *std.Build.Step.Compile = undefined;
var mpq_lib: *std.Build.Step.Compile = undefined;
var utfcpp_lib: *std.Build.Step.Compile = undefined;
var detour_mod_lib: *std.Build.Step.Compile = undefined;
var recast_mod_lib: *std.Build.Step.Compile = undefined;
var fmt_mod_lib: *std.Build.Step.Compile = undefined;
var argon2_lib: *std.Build.Step.Compile = undefined;
var zstd_lib: *std.Build.Step.Compile = undefined;

var boost_path: []const u8 = undefined;

pub fn buildDeps(b: *std.Build, opts: CommonBuildOptions) !void {
    try buildG3DLiteLib(b, opts);
    try buildMpqLib(b, opts);
    try buildUtf8Lib(b, opts);
    try buildDetour(b, opts);
    try buildRecast(b, opts);
    try buildFmt(b, opts);
    try buildArgon2(b, opts);

    const zstd_dep = b.dependency("zstd", .{
        .target = opts.target,
        .optimize = opts.optimize,
    });
    zstd_lib = zstd_dep.artifact("zstd");

    boost_path = opts.boost_path;
}

//
// Links
//

pub fn linkG3DLite(b: *std.Build, target: *std.Build.Module) void {
    target.linkLibrary(g3d_lite_lib);
    for (g3d_lite_lib.root_module.include_dirs.items) |incl| {
        target.addIncludePath(incl.path.dupe(b));
    }
}

pub fn linkMpq(b: *std.Build, target: *std.Build.Module) void {
    target.linkLibrary(g3d_lite_lib);
    for (g3d_lite_lib.root_module.include_dirs.items) |incl| {
        target.addIncludePath(incl.path.dupe(b));
    }
}

pub fn linkDetour(b: *std.Build, target: *std.Build.Module) void {
    target.linkLibrary(detour_mod_lib);
    for (detour_mod_lib.root_module.include_dirs.items) |incl| {
        target.addIncludePath(incl.path.dupe(b));
    }
}

pub fn linkRecast(b: *std.Build, target: *std.Build.Module) void {
    target.linkLibrary(recast_mod_lib);
    for (recast_mod_lib.root_module.include_dirs.items) |incl| {
        target.addIncludePath(incl.path.dupe(b));
    }
}

pub fn linkFmt(b: *std.Build, target: *std.Build.Module) void {
    target.linkLibrary(fmt_mod_lib);
    for (fmt_mod_lib.root_module.include_dirs.items) |incl| {
        target.addIncludePath(incl.path.dupe(b));
    }
    target.addCMacro("FMT_CONSTEVAL", " ");
}

pub fn linkArgon2(b: *std.Build, target: *std.Build.Module) void {
    target.linkLibrary(argon2_lib);
    for (argon2_lib.root_module.include_dirs.items) |incl| {
        target.addIncludePath(incl.path.dupe(b));
    }
}

pub fn linkBoost(b: *std.Build, target: *std.Build.Module) void {
    const boost_include_path = std.mem.concat(b.allocator, u8, &.{ boost_path, "/include" }) catch {
        @panic("oom");
    };
    target.addIncludePath(.{ .cwd_relative = boost_include_path });
}

pub fn linkUtf8(b: *std.Build, target: *std.Build.Module) void {
    for (utfcpp_lib.root_module.include_dirs.items) |incl| {
        target.addIncludePath(incl.path.dupe(b));
    }
    target.linkLibrary(utfcpp_lib);
}

pub fn linkZstd(b: *std.Build, target: *std.Build.Module) void {
    for (zstd_lib.root_module.include_dirs.items) |incl| {
        target.addIncludePath(incl.path.dupe(b));
    }
    target.linkLibrary(zstd_lib);
}

//
// Builds
//
//

fn buildArgon2(b: *std.Build, options: CommonBuildOptions) !void {
    const mod = b.createModule(.{
        .target = options.target,
        .optimize = options.optimize,
        .link_libcpp = true,
    });

    mod.addIncludePath(b.path("./deps/argon2"));
    mod.addIncludePath(b.path("./deps/argon2/argon2"));
    mod.addIncludePath(b.path("./deps/argon2/argon2/blake2"));

    const srcs = try utils.getAllSources(b.allocator, "./deps/argon2/argon2", utils.JUST_C);
    for (srcs.items) |src| {
        mod.addCSourceFile(.{
            .file = b.path(src),
            .flags = &.{"-std=c11"},
        });
    }

    argon2_lib = b.addLibrary(.{
        .name = "argon2",
        .root_module = mod,
    });
}

fn buildG3DLiteLib(b: *std.Build, options: CommonBuildOptions) !void {
    const g3d_lite_mod = b.createModule(.{
        .target = options.target,
        .optimize = options.optimize,
        .link_libcpp = true,
    });

    g3d_lite_mod.addIncludePath(b.path("./deps/g3dlite/include"));

    const g3d_srcs = try utils.getPrefixedCppSrcs(b.allocator, "./deps/g3dlite/", G3D_LITE_SRCS);
    for (g3d_srcs.items) |src| {
        g3d_lite_mod.addCSourceFile(.{
            .file = b.path(src),
            .flags = &.{"-std=c++20"},
        });
    }

    g3d_lite_mod.linkSystemLibrary("zlib", .{});

    g3d_lite_lib = b.addLibrary(.{
        .name = "g3d_lite",
        .root_module = g3d_lite_mod,
    });
}

fn buildMpqLib(b: *std.Build, options: CommonBuildOptions) !void {
    const gpa = b.allocator;
    const libMPQ_mod = b.createModule(.{
        .target = options.target,
        .optimize = options.optimize,
        .link_libcpp = true,
    });

    libMPQ_mod.addIncludePath(b.path("./deps/libmpq/libmpq"));

    const libMPQ_srcs = try utils.getAllSources(gpa, "./deps/libmpq/libmpq", utils.JUST_C);

    for (libMPQ_srcs.items) |src| {
        libMPQ_mod.addCSourceFile(
            .{
                .file = b.path(src),
                .flags = &.{"-std=c89"},
            },
        );
    }

    libMPQ_mod.linkSystemLibrary("zlib", .{});
    libMPQ_mod.linkSystemLibrary("bzip2", .{});

    const libMPQ_config_header = std.Build.Step.ConfigHeader.create(b, .{});
    libMPQ_config_header.addValue("HAVE_LIBZ", i64, 1);
    libMPQ_config_header.addValue("PACKAGE", []const u8, "libmpq");
    libMPQ_config_header.addValue("PACKAGE_VERSION", []const u8, "0.4.2");
    libMPQ_config_header.addValue("VERSION", []const u8, "0.4.2");
    libMPQ_mod.addConfigHeader(libMPQ_config_header);

    mpq_lib = b.addLibrary(.{
        .name = "mpq",
        .root_module = libMPQ_mod,
    });
}

fn buildUtf8Lib(b: *std.Build, options: CommonBuildOptions) !void {
    const gpa = b.allocator;
    const utf8cpp_mod = b.createModule(.{
        .target = options.target,
        .optimize = options.optimize,
        .link_libcpp = true,
    });

    utf8cpp_mod.addIncludePath(b.path("./deps/utf8cpp"));

    var utf8_srcs = try utils.getAllSources(gpa, "./deps/utf8cpp/utf8", utils.JUST_H);
    utils.filterOutFile(&utf8_srcs, "cpp17.h");
    for (utf8_srcs.items) |src| {
        utf8cpp_mod.addCSourceFile(
            .{
                .file = b.path(src),
                .flags = &.{"-std=c++11"},
                .language = .cpp,
            },
        );
    }

    utfcpp_lib = b.addLibrary(.{
        .name = "utf8cpp",
        .root_module = utf8cpp_mod,
    });
}

fn buildDetour(b: *std.Build, options: CommonBuildOptions) !void {
    const gpa = b.allocator;

    const recast_detour_mod = b.createModule(.{
        .target = options.target,
        .optimize = options.optimize,
        .link_libcpp = true,
    });

    recast_detour_mod.addIncludePath(b.path("./deps/recastnavigation/Detour/Include"));

    const srcs = try utils.getAllSources(gpa, "./deps/recastnavigation/Detour/Source", utils.JUST_CPP);

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
}

fn buildRecast(b: *std.Build, options: CommonBuildOptions) !void {
    const gpa = b.allocator;

    const recast_mod = b.createModule(.{
        .target = options.target,
        .optimize = options.optimize,
        .link_libcpp = true,
    });

    recast_mod.addIncludePath(b.path("./deps/recastnavigation/Recast/Include"));

    const srcs = try utils.getAllSources(gpa, "./deps/recastnavigation/Recast/Source", utils.JUST_CPP);

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

    const srcs = try utils.getAllSources(gpa, "./deps/fmt/src/", utils.JUST_CC);

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

// Manual source sets

const G3D_LITE_SRCS: []const []const u8 = &.{
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
