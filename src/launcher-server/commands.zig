//! Command handlers for the launcher RPC server.
//!
//! Handlers only see a typed request and return a `Response`; framing, JSON
//! envelopes, body lengths and flushing live in `session.zig`. That keeps the
//! command code a simple request -> response mapping.

const std = @import("std");

const protocol = @import("protocol.zig");

pub const Context = struct {
    /// Directory exposed by `get_file`, opened once at startup. `null` means
    /// file serving is disabled (`--root` was not passed).
    root: ?std.Io.Dir,
};

pub const Failure = struct {
    code: []const u8,
    message: []const u8,
};

pub const FileBody = struct {
    file: std.Io.File,
    size: u64,
};

/// What a command handler can return. `session.zig` turns this into one wire
/// frame, including the file body when present.
pub const Response = union(enum) {
    ping: protocol.PongPayload,
    echo: protocol.EchoResponse,
    file: FileBody,
    failure: Failure,
};

pub fn handle(
    ctx: Context,
    io: std.Io,
    request: protocol.Request,
    allocator: std.mem.Allocator,
) Response {
    return switch (request.cmd) {
        .ping => .{ .ping = .{} },
        .echo => echo(request, allocator),
        .get_file => getFile(ctx, io, request, allocator),
    };
}

fn echo(request: protocol.Request, allocator: std.mem.Allocator) Response {
    const payload = protocol.parsePayload(protocol.EchoPayload, allocator, request.payload) catch |err| {
        return invalidPayload("expected {\"text\": string}", err);
    };
    return .{ .echo = .{ .text = payload.text } };
}

fn getFile(
    ctx: Context,
    io: std.Io,
    request: protocol.Request,
    allocator: std.mem.Allocator,
) Response {
    const root = ctx.root orelse return failure(
        "file_serving_disabled",
        "start the server with --root to enable file downloads",
    );

    const payload = protocol.parsePayload(protocol.GetFilePayload, allocator, request.payload) catch |err| {
        return invalidPayload("expected {\"path\": string}", err);
    };

    if (!isSafeRelativePath(payload.path)) return failure(
        "invalid_path",
        "path must be relative and must not contain '\\', ':', empty, '.' or '..' components",
    );

    var file = openRegularFile(root, io, payload.path) catch |err| return .{ .failure = openFailure(err) };
    const stat = file.stat(io) catch |err| {
        file.close(io);
        return .{ .failure = openFailure(err) };
    };

    return .{ .file = .{ .file = file, .size = stat.size } };
}

fn failure(code: []const u8, message: []const u8) Response {
    return .{ .failure = .{ .code = code, .message = message } };
}

fn invalidPayload(shape: []const u8, err: anyerror) Response {
    return switch (err) {
        error.OutOfMemory => failure("internal_error", "out of memory"),
        else => failure("invalid_payload", shape),
    };
}

fn openFailure(err: anyerror) Failure {
    return switch (err) {
        error.FileNotFound => .{ .code = "not_found", .message = "file not found" },
        error.AccessDenied, error.PermissionDenied => .{ .code = "access_denied", .message = "access denied" },
        error.IsDir => .{ .code = "is_directory", .message = "path is a directory" },
        error.NotDir => .{ .code = "not_a_directory", .message = "path component is not a directory" },
        error.SymLinkLoop => .{ .code = "access_denied", .message = "symlinks are not served" },
        error.NotRegularFile => .{ .code = "not_a_file", .message = "path is not a regular file" },
        error.NameTooLong => .{ .code = "invalid_path", .message = "path is too long" },
        else => .{ .code = "io_error", .message = "could not read file" },
    };
}

/// Opens a regular file below `root` without following *any* symlink.
///
/// `resolve_beneath` is not available on Linux (openat2's RESOLVE_BENEATH is
/// not an `O_*` flag), so a symlink inside the root could otherwise expose
/// files outside it. Walk each directory component with `follow_symlinks =
/// false`, then stat/open the final component the same way.
fn openRegularFile(root: std.Io.Dir, io: std.Io, path: []const u8) !std.Io.File {
    var dir = root;
    var opened_dir: ?std.Io.Dir = null;
    defer if (opened_dir) |opened| opened.close(io);

    var components = std.mem.splitScalar(u8, path, '/');
    var component = components.next() orelse return error.FileNotFound;
    while (components.next()) |next| {
        const sub = try dir.openDir(io, component, .{ .follow_symlinks = false });
        if (opened_dir) |opened| opened.close(io);
        opened_dir = sub;
        dir = sub;
        component = next;
    }

    // Reject directories, symlinks and FIFOs before opening. Opening a FIFO
    // would block, and a symlink must not be followed out of the root.
    const stat = try dir.statFile(io, component, .{ .follow_symlinks = false });
    if (stat.kind == .directory) return error.IsDir;
    if (stat.kind != .file) return error.NotRegularFile;

    return dir.openFile(io, component, .{
        .allow_directory = false,
        .follow_symlinks = false,
    });
}

/// Lexical guard for paths inside the `--root` directory. `openRegularFile`
/// then walks the components without following symlinks as a second defence.
pub fn isSafeRelativePath(path: []const u8) bool {
    if (path.len == 0) return false;
    if (std.fs.path.isAbsolute(path)) return false;
    if (std.mem.indexOfScalar(u8, path, 0) != null) return false;
    // Reject Windows volume-relative paths (`C:foo` is not absolute but
    // resolves against another drive's cwd) and alternate-data-stream syntax.
    if (std.mem.indexOfScalar(u8, path, ':')) |_| return false;
    // Backslashes are separators on Windows but ordinary characters on POSIX;
    // reject them so a launcher request means the same thing on both.
    if (std.mem.indexOfScalar(u8, path, '\\') != null) return false;

    var components = std.mem.splitScalar(u8, path, '/');
    while (components.next()) |component| {
        if (component.len == 0) return false;
        if (std.mem.eql(u8, component, ".")) return false;
        if (std.mem.eql(u8, component, "..")) return false;
    }
    return true;
}

test "safe relative paths" {
    try std.testing.expect(isSafeRelativePath("launcher.exe"));
    try std.testing.expect(isSafeRelativePath("addons/Interface/Foo.toc"));
    try std.testing.expect(isSafeRelativePath("a/b/c.bin"));
}

test "unsafe paths are rejected" {
    try std.testing.expect(!isSafeRelativePath(""));
    try std.testing.expect(!isSafeRelativePath("/etc/passwd"));
    try std.testing.expect(!isSafeRelativePath("../secret"));
    try std.testing.expect(!isSafeRelativePath("a/../../b"));
    try std.testing.expect(!isSafeRelativePath("./a"));
    try std.testing.expect(!isSafeRelativePath("a//b"));
    try std.testing.expect(!isSafeRelativePath("a/"));
    try std.testing.expect(!isSafeRelativePath("a\\b"));
    try std.testing.expect(!isSafeRelativePath("C:foo"));
    try std.testing.expect(!isSafeRelativePath("file.txt:stream"));
    try std.testing.expect(!isSafeRelativePath("a\x00b"));
}
