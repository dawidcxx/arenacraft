//! Wire format for `ac launcher-server`.
//!
//! A connection carries any number of request/response frames, processed in
//! order. Each frame is:
//!
//!   <json envelope>\n
//!   <body_len raw bytes>
//!
//! The envelope is one line of JSON, so it is easy to log and parse. The
//! optional body is *not* line-framed: it is exactly `body_len` opaque bytes,
//! which makes file contents (NULs, newlines, anything) safe without base64.
//! The next envelope starts immediately after the body.
//!
//! The envelope for a request:
//!
//!   {"id": 7, "cmd": "get_file", "payload": {"path": "a.zip"}, "body_len": 0}
//!
//! and for a reply:
//!
//!   {"id": 7, "cmd": "get_file", "ok": true, "payload": {"size": 1234}, "body_len": 1234}
//!   <1234 raw file bytes>
//!
//! `id` is echoed verbatim so a client can pipeline requests later. Request
//! bodies are reserved by the envelope but not supported yet: a non-zero
//! `body_len` is answered with an `unexpected_body` error and the connection
//! is closed (so a partial body cannot desynchronize the stream).

const std = @import("std");

/// Maximum envelope line, including the trailing newline. `takeDelimiter`
/// fails with `StreamTooLong` instead of growing memory.
pub const max_header_len = 16 * 1024;

pub const Command = enum {
    ping,
    echo,
    get_file,
};

pub const Request = struct {
    id: u64,
    cmd: Command,
    payload: std.json.Value,
    body_len: u64,
};

pub const UnknownCommand = struct {
    id: u64,
    /// Arena-owned, valid until the request is handled.
    cmd: []const u8,
    body_len: u64,
};

pub const ReadOutcome = union(enum) {
    eof,
    request: Request,
    unknown_command: UnknownCommand,
};

pub const ReadError = error{
    HeaderTooLong,
    Malformed,
} || std.Io.Reader.Error || error{OutOfMemory};

const RawRequest = struct {
    id: u64 = 0,
    cmd: []const u8,
    payload: std.json.Value = .null,
    body_len: u64 = 0,
};

/// Reads one request. Blank lines are ignored. `allocator` should be a
/// per-request arena; every string in the returned request is allocated there
/// (the reader's buffer is reused for the next header and body).
pub fn readRequest(reader: *std.Io.Reader, allocator: std.mem.Allocator) ReadError!ReadOutcome {
    while (true) {
        const line = reader.takeDelimiter('\n') catch |err| switch (err) {
            error.StreamTooLong => return error.HeaderTooLong,
            error.ReadFailed => return error.ReadFailed,
        } orelse return .eof;

        // Tolerate CRLF from Windows clients.
        const trimmed = std.mem.trimEnd(u8, line, "\r");
        if (trimmed.len == 0) continue;

        const raw = std.json.parseFromSliceLeaky(RawRequest, allocator, trimmed, .{
            // The reader buffer is reused as soon as the body is streamed.
            .allocate = .alloc_always,
            .ignore_unknown_fields = true,
        }) catch |err| switch (err) {
            error.OutOfMemory => return error.OutOfMemory,
            else => return error.Malformed,
        };

        const cmd = std.meta.stringToEnum(Command, raw.cmd) orelse
            return .{ .unknown_command = .{
                .id = raw.id,
                .cmd = raw.cmd,
                .body_len = raw.body_len,
            } };

        return .{ .request = .{
            .id = raw.id,
            .cmd = cmd,
            .payload = raw.payload,
            .body_len = raw.body_len,
        } };
    }
}

pub const ErrorInfo = struct {
    code: []const u8,
    message: []const u8,
};

pub const PongPayload = struct {
    message: []const u8 = "pong",
};

pub const EchoPayload = struct {
    text: []const u8,
};

pub const EchoResponse = EchoPayload;

pub const GetFilePayload = struct {
    path: []const u8,
};

pub const GetFileResponse = struct {
    size: u64,
};

pub fn parsePayload(
    comptime T: type,
    allocator: std.mem.Allocator,
    payload: std.json.Value,
) !T {
    return std.json.parseFromValueLeaky(T, allocator, payload, .{ .ignore_unknown_fields = true });
}

/// Writes one success envelope line. The caller is responsible for writing
/// exactly `body_len` body bytes afterwards (if any) and flushing.
pub fn writeHeader(
    writer: *std.Io.Writer,
    id: u64,
    cmd: Command,
    payload: anytype,
    body_len: u64,
) !void {
    var stringify: std.json.Stringify = .{ .writer = writer };
    try stringify.beginObject();
    try stringify.objectField("id");
    try stringify.write(id);
    try stringify.objectField("cmd");
    try stringify.write(@tagName(cmd));
    try stringify.objectField("ok");
    try stringify.write(true);
    try stringify.objectField("payload");
    try stringify.write(payload);
    try stringify.objectField("body_len");
    try stringify.write(body_len);
    try stringify.endObject();
    try writer.writeByte('\n');
}

/// Writes one error envelope line. `cmd_name` is a raw command name so unknown
/// commands can be reported too.
pub fn writeError(
    writer: *std.Io.Writer,
    id: u64,
    cmd_name: []const u8,
    code: []const u8,
    message: []const u8,
) !void {
    var stringify: std.json.Stringify = .{ .writer = writer };
    try stringify.beginObject();
    try stringify.objectField("id");
    try stringify.write(id);
    try stringify.objectField("cmd");
    try stringify.write(cmd_name);
    try stringify.objectField("ok");
    try stringify.write(false);
    try stringify.objectField("error");
    try stringify.write(ErrorInfo{ .code = code, .message = message });
    try stringify.objectField("body_len");
    try stringify.write(@as(u64, 0));
    try stringify.endObject();
    try writer.writeByte('\n');
}

test "readRequest parses an envelope" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();

    var reader = std.Io.Reader.fixed(
        "{\"id\":7,\"cmd\":\"get_file\",\"payload\":{\"path\":\"addons/a.zip\"},\"body_len\":0}\r\n",
    );
    const outcome = try readRequest(&reader, arena.allocator());
    const request = switch (outcome) {
        .request => |request| request,
        else => return error.TestUnexpectedResult,
    };

    try std.testing.expectEqual(@as(u64, 7), request.id);
    try std.testing.expectEqual(Command.get_file, request.cmd);
    try std.testing.expectEqual(@as(u64, 0), request.body_len);

    const payload = try parsePayload(GetFilePayload, arena.allocator(), request.payload);
    try std.testing.expectEqualStrings("addons/a.zip", payload.path);
}

test "readRequest reports unknown commands and clean EOF" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();

    var reader = std.Io.Reader.fixed("\n{\"id\":3,\"cmd\":\"nope\"}\n");
    const outcome = try readRequest(&reader, arena.allocator());
    const unknown = switch (outcome) {
        .unknown_command => |unknown| unknown,
        else => return error.TestUnexpectedResult,
    };
    try std.testing.expectEqual(@as(u64, 3), unknown.id);
    try std.testing.expectEqualStrings("nope", unknown.cmd);

    const eof = try readRequest(&reader, arena.allocator());
    try std.testing.expectEqual(std.meta.Tag(ReadOutcome).eof, std.meta.activeTag(eof));
}

test "writeHeader is one newline-terminated line" {
    var out: std.Io.Writer.Allocating = .init(std.testing.allocator);
    defer out.deinit();

    try writeHeader(&out.writer, 7, .ping, PongPayload{}, 0);
    try std.testing.expectEqualStrings(
        "{\"id\":7,\"cmd\":\"ping\",\"ok\":true,\"payload\":{\"message\":\"pong\"},\"body_len\":0}\n",
        out.written(),
    );
}

test "writeError carries the error object" {
    var out: std.Io.Writer.Allocating = .init(std.testing.allocator);
    defer out.deinit();

    try writeError(&out.writer, 2, "nope", "unknown_command", "unknown command");
    try std.testing.expectEqualStrings(
        "{\"id\":2,\"cmd\":\"nope\",\"ok\":false,\"error\":{\"code\":\"unknown_command\",\"message\":\"unknown command\"},\"body_len\":0}\n",
        out.written(),
    );
}
