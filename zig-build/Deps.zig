const std = @import("std");
const Build = std.Build;

const cpp = @import("./cppkit-zig/build.zig");
const AcGraph = @import("./BuildCommons.zig").AcGraph;
const BuildRequest = @import("./BuildCommons.zig").BuildRequest;

pub const Deps = struct {
    utf8: struct {
        module: *Build.Module = undefined,
        library: *Build.Step.Compile = undefined,
    } = .{},

    argon2: struct {
        module: *Build.Module = undefined,
        library: *Build.Step.Compile = undefined,
    } = .{},

    detour: struct {
        module: *Build.Module = undefined,
        library: *Build.Step.Compile = undefined,
    } = .{},

    fmt: struct {
        module: *Build.Module = undefined,
        library: *Build.Step.Compile = undefined,
    } = .{},

    boost: struct {
        library: *Build.Step.Compile = undefined,
        include_dirs: []const Build.Module.IncludeDir = &.{},
    } = .{},

    mpq: struct {
        module: *Build.Module = undefined,
        library: *Build.Step.Compile = undefined,
    } = .{},

    recast: struct {
        module: *Build.Module = undefined,
        library: *Build.Step.Compile = undefined,
    } = .{},

    g3dlite: struct {
        module: *Build.Module = undefined,
        library: *Build.Step.Compile = undefined,
    } = .{},

    argparse: struct {
        module: *Build.Module = undefined,
    } = .{},

    back_reference: *AcGraph = undefined,

    const Self = @This();

    pub fn build(self: *Self, b: BuildRequest) !void {
        try self.buildUtf8(b);
        try self.buildArgon2(b);
        try self.buildDetour(b);
        try self.buildFmt(b);
        try self.buildBoost(b);
        try self.buildMpq(b);
        try self.buildRecast(b);
        try self.buildG3DLite(b);
        try self.buildArgparse(b);
    }

    //
    // link helpers - how consumer modules consume a dependency
    //

    pub fn linkUtf8(self: *Self, mod: *Build.Module) void {
        mod.linkLibrary(self.utf8.library);
    }

    pub fn linkArgon2(self: *Self, mod: *Build.Module) void {
        mod.linkLibrary(self.argon2.library);
    }

    pub fn linkDetour(self: *Self, mod: *Build.Module) void {
        mod.linkLibrary(self.detour.library);
    }

    pub fn linkFmt(self: *Self, mod: *Build.Module) void {
        mod.linkLibrary(self.fmt.library);
        // fmt headers must agree with how the library was compiled
        mod.addCMacro("FMT_CONSTEVAL", "constexpr");
    }

    pub fn linkBoost(self: *Self, mod: *Build.Module) void {
        // the boost package does not install headers; its include paths
        // live on the artifact's root module - mirror them over
        for (self.boost.include_dirs) |include_dir| {
            mod.include_dirs.append(self.back_reference.gpa, include_dir) catch @panic("OOM");
        }

        mod.linkLibrary(self.boost.library);

        // defines mirroring deps/boost/CMakeLists.txt
        // NOTE: BOOST_ASIO_NO_DEPRECATED is deliberately NOT set - with
        // asio 1.91 it strips basic_deadline_timer's members, which the
        // common DeadlineTimer wrapper relies on
        mod.addCMacro("BOOST_DATE_TIME_NO_LIB", "1");
        mod.addCMacro("BOOST_REGEX_NO_LIB", "1");
        mod.addCMacro("BOOST_CHRONO_NO_LIB", "1");
        mod.addCMacro("BOOST_SERIALIZATION_NO_LIB", "1");
        mod.addCMacro("BOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE", "1");
        mod.addCMacro("BOOST_SYSTEM_USE_UTF8", "1");
        mod.addCMacro("BOOST_BIND_NO_PLACEHOLDERS", "1");
    }

    pub fn linkMpq(self: *Self, mod: *Build.Module) void {
        mod.linkLibrary(self.mpq.library);
    }

    pub fn linkRecast(self: *Self, mod: *Build.Module) void {
        mod.linkLibrary(self.recast.library);
    }

    pub fn linkG3DLite(self: *Self, mod: *Build.Module) void {
        mod.linkLibrary(self.g3dlite.library);
    }

    pub fn linkArgparse(self: *Self, mod: *Build.Module) void {
        // argparse is header-only: propagate its include paths onto the
        // consumer (same trick as boost, no b needed)
        for (self.argparse.module.include_dirs.items) |include_dir| {
            mod.include_dirs.append(self.back_reference.gpa, include_dir) catch @panic("OOM");
        }
    }

    // unvendored libraries, provided by nix and resolved through pkg-config
    pub fn linkOpenSSL(self: *Self, mod: *Build.Module) void {
        _ = self;
        mod.linkSystemLibrary("openssl", .{});
    }

    pub fn linkHiredis(self: *Self, mod: *Build.Module) void {
        _ = self;
        mod.linkSystemLibrary("hiredis", .{});
    }

    pub fn linkMysqlClient(self: *Self, mod: *Build.Module) void {
        _ = self;
        mod.linkSystemLibrary("mysqlclient", .{});
    }

    pub fn linkZlib(self: *Self, mod: *Build.Module) void {
        _ = self;
        mod.linkSystemLibrary("zlib", .{});
    }

    pub fn linkBzip2(self: *Self, mod: *Build.Module) void {
        _ = self;
        mod.linkSystemLibrary("bzip2", .{});
    }

    //
    // builds
    //

    fn buildUtf8(self: *Self, build_request: BuildRequest) !void {
        const b = build_request[0];
        const target = build_request[1];
        const optimize = build_request[2];

        const alloc = self.back_reference.gpa;
        const io = self.back_reference.io;

        const module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        module.addIncludePath(b.path("deps/utf8cpp"));

        // utf8cpp is header-only: feed the headers to the compiler as
        // translation units so the static library gets objects to link.
        var sources = cpp.querySources(alloc, io, "deps/utf8cpp/utf8", .{
            .extensions = cpp.Exts.JUST_H,
            .recursive = false,
        });

        // uses std::string_view (C++17), incompatible with the c++11 set
        sources.filterOut("cpp17.h");

        module.addCSourceFiles(.{
            .files = sources.get(),
            .language = .cpp,
            .flags = &.{"-std=c++11"},
        });

        const library = b.addLibrary(.{
            .name = "utf8",
            .root_module = module,
            .linkage = .static,
        });

        // expose the headers to every consumer linking this library
        library.installHeadersDirectory(b.path("deps/utf8cpp"), "", .{});

        // update graph at the end
        self.utf8.module = module;
        self.utf8.library = library;
    }

    fn buildArgon2(self: *Self, build_request: BuildRequest) !void {
        const b = build_request[0];
        const target = build_request[1];
        const optimize = build_request[2];

        const alloc = self.back_reference.gpa;
        const io = self.back_reference.io;

        const module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        });

        module.addCMacro("ARGON2_NO_THREADS", "1");

        var sources = cpp.querySources(alloc, io, "deps/argon2", .{
            .extensions = cpp.Exts.JUST_C,
            .recursive = true,
        });

        // mirrors deps/argon2/CMakeLists.txt: pick the implementation
        // matching the target architecture
        if (target.result.cpu.arch == .aarch64) {
            sources.filterOut("opt.c");
        } else {
            sources.filterOut("ref.c");
        }

        module.addCSourceFiles(.{
            .files = sources.get(),
        });

        const library = b.addLibrary(.{
            .name = "argon2",
            .root_module = module,
            .linkage = .static,
        });

        // expose <argon2/argon2.h> to every consumer linking this library
        library.installHeadersDirectory(b.path("deps/argon2"), "", .{});

        // update graph at the end
        self.argon2.module = module;
        self.argon2.library = library;
    }

    fn buildDetour(self: *Self, build_request: BuildRequest) !void {
        const b = build_request[0];
        const target = build_request[1];
        const optimize = build_request[2];

        const alloc = self.back_reference.gpa;
        const io = self.back_reference.io;

        const module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        module.addIncludePath(b.path("deps/recastnavigation/Detour/Include"));

        var sources = cpp.querySources(alloc, io, "deps/recastnavigation/Detour/Source", .{
            .extensions = cpp.Exts.JUST_CPP,
            .recursive = false,
        });

        module.addCSourceFiles(.{
            .files = sources.get(),
            .language = .cpp,
            .flags = &.{"-std=c++14"},
        });

        const library = b.addLibrary(.{
            .name = "detour",
            .root_module = module,
            .linkage = .static,
        });

        // expose <DetourNavMesh.h> & co to every consumer linking this library
        library.installHeadersDirectory(b.path("deps/recastnavigation/Detour/Include"), "", .{});

        // update graph at the end
        self.detour.module = module;
        self.detour.library = library;
    }

    fn buildFmt(self: *Self, build_request: BuildRequest) !void {
        const b = build_request[0];
        const target = build_request[1];
        const optimize = build_request[2];

        const alloc = self.back_reference.gpa;
        const io = self.back_reference.io;

        const module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        module.addCMacro("FMT_CONSTEVAL", "constexpr");
        module.addIncludePath(b.path("deps/fmt/include"));

        // fmt sources are .cc; fmt.cc is the C++20 module interface,
        // excluded just like deps/fmt/CMakeLists.txt does
        var sources = cpp.querySources(alloc, io, "deps/fmt/src", .{
            .extensions = cpp.Exts.JUST_CC,
            .recursive = false,
        });

        sources.filterOut("fmt.cc");

        module.addCSourceFiles(.{
            .files = sources.get(),
            .language = .cpp,
            .flags = &.{"-std=c++20"},
        });

        const library = b.addLibrary(.{
            .name = "fmt",
            .root_module = module,
            .linkage = .static,
        });

        // expose <fmt/format.h> & co to every consumer linking this library
        library.installHeadersDirectory(b.path("deps/fmt/include"), "", .{});

        // update graph at the end
        self.fmt.module = module;
        self.fmt.library = library;
    }

    fn buildBoost(self: *Self, build_request: BuildRequest) !void {
        const b = build_request[0];
        const target = build_request[1];
        const optimize = build_request[2];

        const boost_dep = b.dependency("boost", .{
            .target = target,
            .optimize = optimize,
            .filesystem = true,
        });

        // the artifact bundles the enabled compiled modules; its root module
        // carries the include paths of every header-only boost library
        const library = boost_dep.artifact("boost");

        // update graph at the end
        self.boost.library = library;
        self.boost.include_dirs = library.root_module.include_dirs.items;
    }

    fn buildMpq(self: *Self, build_request: BuildRequest) !void {
        const b = build_request[0];
        const target = build_request[1];
        const optimize = build_request[2];

        const alloc = self.back_reference.gpa;
        const io = self.back_reference.io;

        const module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        });

        // own sources include each other flat, config.h comes from the vendored root
        module.addIncludePath(b.path("deps/libmpq/libmpq"));
        module.addIncludePath(b.path("deps/libmpq"));

        var sources = cpp.querySources(alloc, io, "deps/libmpq/libmpq", .{
            .extensions = cpp.Exts.JUST_C,
            .recursive = false,
        });

        module.addCSourceFiles(.{
            .files = sources.get(),
            .flags = &.{"-std=gnu99"},
        });

        self.linkZlib(module);
        self.linkBzip2(module);

        const library = b.addLibrary(.{
            .name = "mpq",
            .root_module = module,
            .linkage = .static,
        });

        // consumers include <libmpq/mpq.h>
        library.installHeadersDirectory(b.path("deps/libmpq"), "", .{});

        // update graph at the end
        self.mpq.module = module;
        self.mpq.library = library;
    }

    fn buildRecast(self: *Self, build_request: BuildRequest) !void {
        const b = build_request[0];
        const target = build_request[1];
        const optimize = build_request[2];

        const alloc = self.back_reference.gpa;
        const io = self.back_reference.io;

        const module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        module.addIncludePath(b.path("deps/recastnavigation/Recast/Include"));

        var sources = cpp.querySources(alloc, io, "deps/recastnavigation/Recast/Source", .{
            .extensions = cpp.Exts.JUST_CPP,
            .recursive = false,
        });

        module.addCSourceFiles(.{
            .files = sources.get(),
            .language = .cpp,
            .flags = &.{"-std=c++14"},
        });

        const library = b.addLibrary(.{
            .name = "recast",
            .root_module = module,
            .linkage = .static,
        });

        // expose <Recast.h> & co to every consumer linking this library
        library.installHeadersDirectory(b.path("deps/recastnavigation/Recast/Include"), "", .{});

        // update graph at the end
        self.recast.module = module;
        self.recast.library = library;
    }

    fn buildG3DLite(self: *Self, build_request: BuildRequest) !void {
        const b = build_request[0];
        const target = build_request[1];
        const optimize = build_request[2];

        const module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        module.addIncludePath(b.path("deps/g3dlite/include"));

        // source list mirroring deps/g3dlite/CMakeLists.txt (the old zig
        // build carried the same explicit list)
        module.addCSourceFiles(.{
            .root = b.path("deps/g3dlite"),
            .files = &.{
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
            },
            .language = .cpp,
            .flags = &.{"-std=c++20"},
        });

        self.linkZlib(module);

        const library = b.addLibrary(.{
            .name = "g3dlite",
            .root_module = module,
            .linkage = .static,
        });

        // expose <G3D/AABox.h> & co to every consumer linking this library
        library.installHeadersDirectory(b.path("deps/g3dlite/include"), "", .{});

        // update graph at the end
        self.g3dlite.module = module;
        self.g3dlite.library = library;
    }

    fn buildArgparse(self: *Self, build_request: BuildRequest) !void {
        const b = build_request[0];
        const target = build_request[1];
        const optimize = build_request[2];

        const module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        });

        // header-only (p-ranav/argparse), rescued from the old zig experiment
        module.addIncludePath(b.path("deps/argparse/include"));

        // update graph at the end
        self.argparse.module = module;
    }
};
