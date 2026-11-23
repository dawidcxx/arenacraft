//! UTF-8 to UTF-16 conversion for the Win32 wide-character calls.

const std = @import("std");

/// WTF-8 to a null-terminated UTF-16 string, owned by the caller. Out of
/// memory panics, and so does invalid WTF-8: text reaches `gui` from the
/// launcher's own strings and OS-produced paths, so a violation is a bug.
pub fn toWide(allocator: std.mem.Allocator, text: []const u8) [:0]u16 {
    return std.unicode.wtf8ToWtf16LeAllocZ(allocator, text) catch |err| switch (err) {
        error.OutOfMemory => unreachable,
        error.InvalidWtf8 => unreachable,
    };
}
