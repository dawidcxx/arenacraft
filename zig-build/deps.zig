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

    back_reference: *AcGraph = undefined,

    const Self = @This();

    pub fn build(self: *Self, b: BuildRequest) !void {
        try self.buildUtf8(b);
    }

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
};
