//! One client connection. Hides the wire format from the router: `next`
//! returns a parsed request, `reply` serializes a `commands.Response` (header,
//! body, flush) and owns the per-request arena.

const std = @import("std");

const commands = @import("commands.zig");
const protocol = @import("protocol.zig");

pub const Session = struct {
    io: std.Io,
    reader: *std.Io.Reader,
    writer: *std.Io.Writer,
    arena: std.heap.ArenaAllocator,

    pub fn init(
        io: std.Io,
        reader: *std.Io.Reader,
        writer: *std.Io.Writer,
        gpa: std.mem.Allocator,
    ) Session {
        return .{
            .io = io,
            .reader = reader,
            .writer = writer,
            .arena = .init(gpa),
        };
    }

    pub fn deinit(self: *Session) void {
        self.arena.deinit();
    }

    /// Allocations made for the current request; valid until the next `next`.
    pub fn allocator(self: *Session) std.mem.Allocator {
        return self.arena.allocator();
    }

    /// Returns the next known request, or null on clean EOF. Unknown commands
    /// get an error reply and are skipped. Malformed frames and unsupported
    /// request bodies are errors: the caller closes the connection.
    pub fn next(self: *Session) !?protocol.Request {
        while (true) {
            // Reset per frame. Unknown commands loop here, so this must be
            // inside the loop or their parsed strings would accumulate.
            _ = self.arena.reset(.retain_capacity);

            const outcome = try protocol.readRequest(self.reader, self.arena.allocator());
            switch (outcome) {
                .eof => return null,
                .unknown_command => |unknown| {
                    if (unknown.body_len != 0) {
                        try protocol.writeError(
                            self.writer,
                            unknown.id,
                            unknown.cmd,
                            "unexpected_body",
                            "request bodies are not supported yet",
                        );
                        try self.writer.flush();
                        return error.UnexpectedBody;
                    }
                    try protocol.writeError(
                        self.writer,
                        unknown.id,
                        unknown.cmd,
                        "unknown_command",
                        "unknown command",
                    );
                    try self.writer.flush();
                    continue;
                },
                .request => |request| {
                    if (request.body_len != 0) {
                        try protocol.writeError(
                            self.writer,
                            request.id,
                            @tagName(request.cmd),
                            "unexpected_body",
                            "request bodies are not supported yet",
                        );
                        try self.writer.flush();
                        return error.UnexpectedBody;
                    }
                    return request;
                },
            }
        }
    }

    /// Sends exactly one response frame for `request`.
    pub fn reply(self: *Session, request: protocol.Request, response: commands.Response) !void {
        switch (response) {
            .ping => |payload| {
                std.debug.assert(request.cmd == .ping);
                try self.send(request, payload, 0);
            },
            .echo => |payload| {
                std.debug.assert(request.cmd == .echo);
                try self.send(request, payload, 0);
            },
            .file => |body| {
                std.debug.assert(request.cmd == .get_file);
                defer body.file.close(self.io);
                try protocol.writeHeader(
                    self.writer,
                    request.id,
                    request.cmd,
                    protocol.GetFileResponse{ .size = body.size },
                    body.size,
                );
                var buffer: [16 * 1024]u8 = undefined;
                var file_reader = body.file.reader(self.io, &buffer);
                try file_reader.interface.streamExact64(self.writer, body.size);
                try self.writer.flush();
            },
            .failure => |failure| {
                try protocol.writeError(
                    self.writer,
                    request.id,
                    @tagName(request.cmd),
                    failure.code,
                    failure.message,
                );
                try self.writer.flush();
            },
        }
    }

    fn send(self: *Session, request: protocol.Request, payload: anytype, body_len: u64) !void {
        try protocol.writeHeader(self.writer, request.id, request.cmd, payload, body_len);
        try self.writer.flush();
    }
};

test "session maps a request to one response line" {
    var reader = std.Io.Reader.fixed("{\"id\":4,\"cmd\":\"ping\"}\n");
    var out: std.Io.Writer.Allocating = .init(std.testing.allocator);
    defer out.deinit();

    var session = Session.init(std.testing.io, &reader, &out.writer, std.testing.allocator);
    defer session.deinit();

    const request = (try session.next()).?;
    try session.reply(request, .{ .ping = .{} });

    try std.testing.expectEqualStrings(
        "{\"id\":4,\"cmd\":\"ping\",\"ok\":true,\"payload\":{\"message\":\"pong\"},\"body_len\":0}\n",
        out.written(),
    );
}

test "session skips unknown commands and rejects request bodies" {
    var reader = std.Io.Reader.fixed(
        "{\"id\":1,\"cmd\":\"nope\"}\n{\"id\":2,\"cmd\":\"ping\",\"body_len\":3}\n",
    );
    var out: std.Io.Writer.Allocating = .init(std.testing.allocator);
    defer out.deinit();

    var session = Session.init(std.testing.io, &reader, &out.writer, std.testing.allocator);
    defer session.deinit();

    try std.testing.expectError(error.UnexpectedBody, session.next());
    try std.testing.expectEqualStrings(
        "{\"id\":1,\"cmd\":\"nope\",\"ok\":false,\"error\":{\"code\":\"unknown_command\",\"message\":\"unknown command\"},\"body_len\":0}\n" ++
            "{\"id\":2,\"cmd\":\"ping\",\"ok\":false,\"error\":{\"code\":\"unexpected_body\",\"message\":\"request bodies are not supported yet\"},\"body_len\":0}\n",
        out.written(),
    );
}

test "unknown command with a body does not desynchronize the stream" {
    var reader = std.Io.Reader.fixed("{\"id\":9,\"cmd\":\"nope\",\"body_len\":3}\nabc\n");
    var out: std.Io.Writer.Allocating = .init(std.testing.allocator);
    defer out.deinit();

    var session = Session.init(std.testing.io, &reader, &out.writer, std.testing.allocator);
    defer session.deinit();

    try std.testing.expectError(error.UnexpectedBody, session.next());
    try std.testing.expectEqualStrings(
        "{\"id\":9,\"cmd\":\"nope\",\"ok\":false,\"error\":{\"code\":\"unexpected_body\",\"message\":\"request bodies are not supported yet\"},\"body_len\":0}\n",
        out.written(),
    );
}

test "fuzz session with zig's fuzzer" {
    try std.testing.fuzz({}, fuzzSession, .{
        .corpus = &.{
            "",
            "\n",
            "{\"cmd\":\"ping\"}\n",
            "{\"id\":1,\"cmd\":\"echo\",\"payload\":{\"text\":\"hello\"}}\n",
            "{\"id\":2,\"cmd\":\"get_file\",\"payload\":{\"path\":\"addons/a.zip\"}}\n",
            "{\"id\":3,\"cmd\":\"ping\",\"body_len\":1}\n",
            "not json\n",
            "{\"cmd\":\"nope\"}\n",
            "{\"cmd\":\"nope\",\"body_len\":3}\n",
            "{\"id\":4,\"cmd\":\"ping\"}\n{\"id\":5,\"cmd\":\"echo\",\"payload\":{\"text\":\"x\"}}\n",
        },
    });
}

fn fuzzSession(_: void, smith: *std.testing.Smith) !void {
    var input: [4096]u8 = undefined;
    const len = smith.slice(&input);

    var reader = std.Io.Reader.fixed(input[0..len]);
    var out: std.Io.Writer.Allocating = .init(std.testing.allocator);
    defer out.deinit();

    var session = Session.init(std.testing.io, &reader, &out.writer, std.testing.allocator);
    defer session.deinit();

    while (session.next() catch return) |request| {
        const response = commands.handle(.{ .root = null }, std.testing.io, request, session.allocator());
        session.reply(request, response) catch return;
    }
}
