//! Storage for the generated compile command entries.
//!
//! Pure data structures and logic, no build system coupling, so this is the
//! part of the generator that is easy to test in isolation.

const std = @import("std");

pub const CompileCommandJson = struct {
    arguments: []const []const u8,
    directory: []const u8,
    file: []const u8,
    output: []const u8,
};

pub const CompileCommand = struct {
    /// absolute path of the source file
    file_name: []const u8,
    /// absolute directory containing the source file
    directory: []const u8,
    /// ordered, deduplicated compile flags of this file
    flags: std.ArrayListUnmanaged([]const u8) = .empty,

    pub fn lessThan(_: void, a: *CompileCommand, b: *CompileCommand) bool {
        return std.mem.lessThan(u8, a.file_name, b.file_name);
    }

    pub fn toJson(
        self: *const CompileCommand,
        alloc: std.mem.Allocator,
        output_root: []const u8,
        driver: []const u8,
    ) !CompileCommandJson {
        const as_object_file = try std.fmt.allocPrint(alloc, "{s}.o", .{std.fs.path.basename(self.file_name)});
        const output_file = try std.fs.path.join(alloc, &.{ output_root, as_object_file });

        var arguments: std.ArrayListUnmanaged([]const u8) = .empty;
        try arguments.ensureTotalCapacity(alloc, 4 + self.flags.items.len);
        arguments.appendAssumeCapacity(driver);
        arguments.appendAssumeCapacity(self.file_name);
        arguments.appendAssumeCapacity("-o");
        arguments.appendAssumeCapacity(output_file);
        arguments.appendSliceAssumeCapacity(self.flags.items);

        return .{
            .arguments = try arguments.toOwnedSlice(alloc),
            .directory = self.directory,
            .file = self.file_name,
            .output = output_file,
        };
    }
};

pub const CompileCommandsDb = struct {
    arena: std.heap.ArenaAllocator,
    compile_commands_by_file_name: std.StringHashMapUnmanaged(*CompileCommand),

    pub fn init(gpa: std.mem.Allocator) CompileCommandsDb {
        const arena = std.heap.ArenaAllocator.init(gpa);
        return .{
            .arena = arena,
            .compile_commands_by_file_name = .empty,
        };
    }

    pub fn update(
        self: *CompileCommandsDb,
        file_name: []const u8,
        flags: []const []const u8,
    ) !void {
        const alloc = self.arena.allocator();
        const cc_gop = try self.compile_commands_by_file_name.getOrPut(alloc, file_name);
        if (!cc_gop.found_existing) {
            const owned_file_name = try alloc.dupe(u8, file_name);
            cc_gop.key_ptr.* = owned_file_name;
            const cc = try alloc.create(CompileCommand);
            cc.* = .{
                .file_name = owned_file_name,
                .directory = std.fs.path.dirname(owned_file_name) orelse "/",
            };
            cc_gop.value_ptr.* = cc;
        }
        const compile_command = cc_gop.value_ptr.*;
        // Deduplicate flags. Two-token flags ("-I" + path, "--sysroot" + path)
        // are deduplicated as whole pairs, otherwise repeated occurrences of
        // the flag token itself would be stripped and break their arguments.
        var i: usize = 0;
        while (i < flags.len) {
            const flag = flags[i];
            const is_two_token = isTwoTokenFlag(flag) and i + 1 < flags.len;
            const unit_len: usize = if (is_two_token) 2 else 1;

            var exists = false;
            var j: usize = 0;
            while (j + unit_len <= compile_command.flags.items.len) : (j += 1) {
                if (!std.mem.eql(u8, compile_command.flags.items[j], flag)) continue;
                if (unit_len == 2 and !std.mem.eql(u8, compile_command.flags.items[j + 1], flags[i + 1])) continue;
                exists = true;
                break;
            }

            if (!exists) {
                try compile_command.flags.append(alloc, try alloc.dupe(u8, flag));
                if (unit_len == 2) {
                    try compile_command.flags.append(alloc, try alloc.dupe(u8, flags[i + 1]));
                }
            }
            i += unit_len;
        }
    }
};

pub fn isTwoTokenFlag(flag: []const u8) bool {
    const two_token_flags = [_][]const u8{ "-I", "-isystem", "-idirafter", "--sysroot" };
    for (two_token_flags) |two_token_flag| {
        if (std.mem.eql(u8, flag, two_token_flag)) return true;
    }
    return false;
}
