//! Minimal MPQ v1 writer for the launcher patches.
//!
//! We only need the subset the 3.3.5a client reads: a v1 header, a block table,
//! a hash table and file payloads. Every file is stored as one single-unit
//! sector, which lets zlib compress the whole file instead of restarting the
//! dictionary every 4 KiB (important for `Spell.dbc`, ~49 MB with long-range
//! repeats). Files whose compressed form is not smaller are stored raw.
//!
//! The tables are encrypted exactly like libmpq/the client expect; see
//! `deps/libmpq/libmpq/common.c` for the reference implementation. Paths use
//! backslash separators and are hashed case-insensitively, so callers must pass
//! the on-disk name (`DBFilesClient\Spell.dbc`).

const std = @import("std");

// zlib is linked by the `ac` executable (and explicitly by the standalone test
// binary). Declaring the two one-shot entry points avoids a translate-c
// dependency; 0.17 removed `@cImport` from the language.
const z_ok: c_int = 0;
extern "c" fn compressBound(source_len: c_ulong) c_ulong;
extern "c" fn compress2(
    dest: [*]u8,
    dest_len: *c_ulong,
    source: [*]const u8,
    source_len: c_ulong,
    level: c_int,
) c_int;

pub const Error = error{
    ArchiveTooLarge,
    CompressionFailed,
    DuplicatePath,
    InvalidPath,
};

/// One file to put in the archive. `path` is the MPQ path (backslash
/// separated), `data` is the uncompressed content.
pub const File = struct {
    path: []const u8,
    data: []const u8,
};

const header_size = 32;
const sector_size = 512;
const table_entry_size = 16;

const flag_exists: u32 = 0x8000_0000;
const flag_single_unit: u32 = 0x0100_0000;
/// `MPQ_FILE_COMPRESS`: the payload starts with a compression-method byte and
/// the client applies the methods named in that byte.
const flag_compress: u32 = 0x0000_0200;
const compression_zlib: u8 = 0x02;

const hash_free = 0xFFFF_FFFF;

const Entry = struct {
    path: []const u8,
    payload: []const u8,
    compressed: bool,
    offset: u32 = 0,

    /// Size of the payload written to the archive, including the leading
    /// compression-method byte for compressed entries.
    fn packedSize(self: Entry) u32 {
        return @intCast(self.payload.len + @intFromBool(self.compressed));
    }
};

/// Builds one complete MPQ archive in memory. The returned slice is owned by
/// `allocator`; the caller frees it after the archive is no longer needed.
pub fn build(allocator: std.mem.Allocator, files: []const File) ![]u8 {
    var arena = std.heap.ArenaAllocator.init(allocator);
    defer arena.deinit();
    const scratch = arena.allocator();

    const entries = try scratch.alloc(Entry, files.len);
    const unpacked_sizes = try scratch.alloc(u32, files.len);

    for (files, 0..) |file, i| {
        if (!isArchivePath(file.path)) return error.InvalidPath;
        if (file.data.len > std.math.maxInt(u32)) return error.ArchiveTooLarge;

        const compressed = try compressZlib(scratch, file.data);
        entries[i] = .{
            .path = file.path,
            .payload = compressed orelse file.data,
            .compressed = compressed != null,
        };
        unpacked_sizes[i] = @intCast(file.data.len);
    }

    // Lay the payloads out first (sector-aligned so the client's usual
    // sector reads stay aligned), then the block table and the hash table.
    var cursor: usize = sector_size;
    for (entries) |*entry| {
        cursor = std.mem.alignForward(usize, cursor, sector_size);
        if (cursor > std.math.maxInt(u32)) return error.ArchiveTooLarge;
        entry.offset = @intCast(cursor);
        cursor += entry.packedSize();
    }

    const block_table_offset = std.mem.alignForward(usize, cursor, sector_size);
    const hash_count: u32 = std.math.ceilPowerOfTwoAssert(u32, @max(@as(u32, @intCast(files.len)), 4));
    const hash_table_offset = std.mem.alignForward(usize, block_table_offset + files.len * table_entry_size, sector_size);
    const archive_size = std.mem.alignForward(usize, hash_table_offset + hash_count * table_entry_size, sector_size);
    if (archive_size > std.math.maxInt(u32)) return error.ArchiveTooLarge;

    const out = try allocator.alloc(u8, archive_size);
    errdefer allocator.free(out);
    @memset(out, 0);

    writeHeader(out, .{
        .archive_size = @intCast(archive_size),
        .hash_table_offset = @intCast(hash_table_offset),
        .block_table_offset = @intCast(block_table_offset),
        .hash_table_count = hash_count,
        .block_table_count = @intCast(files.len),
    });

    for (entries, 0..) |entry, i| {
        if (entry.compressed) {
            out[entry.offset] = compression_zlib;
            @memcpy(out[entry.offset + 1 ..][0..entry.payload.len], entry.payload);
        } else {
            @memcpy(out[entry.offset..][0..entry.payload.len], entry.payload);
        }
        writeBlockEntry(out[block_table_offset + i * table_entry_size ..][0..table_entry_size], .{
            .offset = entry.offset,
            .packed_size = entry.packedSize(),
            .unpacked_size = unpacked_sizes[i],
            .flags = flag_exists | flag_single_unit | (if (entry.compressed) flag_compress else 0),
        });
    }
    encryptBlock(hashString("(block table)", 0x300), out[block_table_offset..][0 .. files.len * table_entry_size]);

    const hash_entries = try scratch.alloc(HashEntry, hash_count);
    for (hash_entries) |*hash| hash.* = .empty;
    for (entries, 0..) |entry, i| {
        const name_a = hashString(entry.path, 0x100);
        const name_b = hashString(entry.path, 0x200);
        var slot: u32 = hashString(entry.path, 0x000) & (hash_count - 1);
        while (true) {
            const hash = &hash_entries[slot];
            if (hash.block_index == hash_free) {
                hash.* = .{
                    .name_a = name_a,
                    .name_b = name_b,
                    .block_index = @intCast(i),
                };
                break;
            }
            // The client resolves files by (name_a, name_b); two different
            // spellings that hash the same are ambiguous, so refuse them.
            if (hash.name_a == name_a and hash.name_b == name_b) return error.DuplicatePath;
            slot = (slot + 1) & (hash_count - 1);
        }
    }
    for (hash_entries, 0..) |hash, i| {
        writeHashEntry(out[hash_table_offset + i * table_entry_size ..][0..table_entry_size], hash);
    }
    encryptBlock(hashString("(hash table)", 0x300), out[hash_table_offset..][0 .. hash_count * table_entry_size]);

    return out;
}

const Header = struct {
    archive_size: u32,
    hash_table_offset: u32,
    block_table_offset: u32,
    hash_table_count: u32,
    block_table_count: u32,
};

fn writeHeader(out: []u8, header: Header) void {
    std.debug.assert(out.len >= header_size);
    std.mem.writeInt(u32, out[0..4], 0x1A51_504D, .little); // "MPQ\x1A"
    std.mem.writeInt(u32, out[4..8], header_size, .little);
    std.mem.writeInt(u32, out[8..12], header.archive_size, .little);
    std.mem.writeInt(u16, out[12..14], 0, .little); // format version 1
    std.mem.writeInt(u16, out[14..16], 3, .little); // 512 << 3 = 4 KiB sectors
    std.mem.writeInt(u32, out[16..20], header.hash_table_offset, .little);
    std.mem.writeInt(u32, out[20..24], header.block_table_offset, .little);
    std.mem.writeInt(u32, out[24..28], header.hash_table_count, .little);
    std.mem.writeInt(u32, out[28..32], header.block_table_count, .little);
}

const BlockEntry = struct {
    offset: u32,
    packed_size: u32,
    unpacked_size: u32,
    flags: u32,
};

fn writeBlockEntry(out: *[table_entry_size]u8, entry: BlockEntry) void {
    std.mem.writeInt(u32, out[0..4], entry.offset, .little);
    std.mem.writeInt(u32, out[4..8], entry.packed_size, .little);
    std.mem.writeInt(u32, out[8..12], entry.unpacked_size, .little);
    std.mem.writeInt(u32, out[12..16], entry.flags, .little);
}

const HashEntry = struct {
    name_a: u32,
    name_b: u32,
    block_index: u32,

    const empty: HashEntry = .{
        .name_a = hash_free,
        .name_b = hash_free,
        .block_index = hash_free,
    };
};

fn writeHashEntry(out: *[table_entry_size]u8, entry: HashEntry) void {
    std.mem.writeInt(u32, out[0..4], entry.name_a, .little);
    std.mem.writeInt(u32, out[4..8], entry.name_b, .little);
    // Locale/platform 0 = neutral. Empty entries use all-ones here as well.
    const empty = entry.block_index == hash_free;
    std.mem.writeInt(u16, out[8..10], if (empty) 0xFFFF else 0, .little);
    std.mem.writeInt(u16, out[10..12], if (empty) 0xFFFF else 0, .little);
    std.mem.writeInt(u32, out[12..16], entry.block_index, .little);
}

/// MPQ path rules: backslash separated, no absolute paths, no `.`/`..`, no
/// alternate data streams, no empty components.
pub fn isArchivePath(path: []const u8) bool {
    if (path.len == 0) return false;
    if (path[0] == '\\' or path[path.len - 1] == '\\') return false;
    if (std.mem.indexOfScalar(u8, path, 0) != null) return false;
    if (std.mem.indexOfScalar(u8, path, ':') != null) return false;
    if (std.mem.indexOfScalar(u8, path, '/') != null) return false;

    var components = std.mem.splitScalar(u8, path, '\\');
    while (components.next()) |component| {
        if (component.len == 0) return false;
        if (std.mem.eql(u8, component, ".")) return false;
        if (std.mem.eql(u8, component, "..")) return false;
    }
    return true;
}

fn compressZlib(allocator: std.mem.Allocator, data: []const u8) !?[]u8 {
    // An empty file compresses to a zlib header + footer, which is larger than
    // storing nothing.
    if (data.len == 0) return null;

    // System zlib: the same implementation libmpq/the client use, and much
    // faster than the Zig compressor for the ~49 MB Spell.dbc (especially in
    // Debug builds). `compressBound` guarantees the destination is large
    // enough; the level is zlib's maximum.
    const bound = compressBound(@intCast(data.len));
    const out = try allocator.alloc(u8, @intCast(bound));
    errdefer allocator.free(out);

    var out_len: c_ulong = bound;
    if (compress2(out.ptr, &out_len, data.ptr, @intCast(data.len), 9) != z_ok) {
        return error.CompressionFailed;
    }
    if (out_len >= data.len) {
        allocator.free(out);
        return null;
    }
    return try allocator.realloc(out, @intCast(out_len));
}

/// MPQ hash table. `offset` selects the key type: 0x000 for the table index,
/// 0x100/0x200 for the two stored name hashes, 0x300 for encryption keys.
pub fn hashString(path: []const u8, offset: usize) u32 {
    var seed1: u32 = 0x7FED_7FED;
    var seed2: u32 = 0xEEEE_EEEE;
    for (path) |byte| {
        const ch: u32 = std.ascii.toUpper(byte);
        seed1 = crypt_table[offset + ch] ^ (seed1 +% seed2);
        seed2 = ch +% seed1 +% seed2 +% (seed2 << 5) +% 3;
    }
    return seed1;
}

/// In-place MPQ block encryption (the algorithm is symmetric only in shape;
/// this direction is what the client decrypts).
fn encryptBlock(seed_start: u32, bytes: []u8) void {
    std.debug.assert(bytes.len % 4 == 0);
    var seed = seed_start;
    var seed2: u32 = 0xEEEE_EEEE;
    var i: usize = 0;
    while (i < bytes.len) : (i += 4) {
        const value = std.mem.readInt(u32, bytes[i..][0..4], .little);
        seed2 +%= crypt_table[0x400 + (seed & 0xFF)];
        const encrypted = value ^ (seed +% seed2);
        seed = ((~seed << 21) +% 0x1111_1111) | (seed >> 11);
        seed2 = value +% seed2 +% (seed2 << 5) +% 3;
        std.mem.writeInt(u32, bytes[i..][0..4], encrypted, .little);
    }
}

/// StormLib's crypt table, generated at comptime (1280 u32 entries).
const crypt_table: [0x500]u32 = blk: {
    @setEvalBranchQuota(10_000);
    var table: [0x500]u32 = undefined;
    var seed: u32 = 0x0010_0001;
    for (0..0x100) |i| {
        var index = i;
        for (0..5) |_| {
            seed = (seed *% 125 +% 3) % 0x2A_AAAB;
            const high: u32 = (seed & 0xFFFF) << 16;
            seed = (seed *% 125 +% 3) % 0x2A_AAAB;
            table[index] = high | (seed & 0xFFFF);
            index += 0x100;
        }
    }
    break :blk table;
};

test "crypt table matches libmpq" {
    // deps/libmpq/libmpq/crypt_buf.h
    try std.testing.expectEqual(@as(u32, 0x55C6_36E2), crypt_table[0]);
    try std.testing.expectEqual(@as(u32, 0x02BE_0170), crypt_table[1]);
    try std.testing.expectEqual(@as(u32, 0x7303_286C), crypt_table[0x4FF]);
}

test "hash strings match libmpq" {
    try std.testing.expectEqual(@as(u32, 0xF5C5_EFE2), hashString("(hash table)", 0x000));
    try std.testing.expectEqual(@as(u32, 0xC3AF_3770), hashString("(hash table)", 0x300));
    try std.testing.expectEqual(@as(u32, 0xEC83_B3A3), hashString("(block table)", 0x300));
    try std.testing.expectEqual(@as(u32, 0x1096_2C85), hashString("DBFilesClient\\Spell.dbc", 0x000));
    try std.testing.expectEqual(@as(u32, 0xCCE8_E4E4), hashString("DBFilesClient\\Spell.dbc", 0x100));
    try std.testing.expectEqual(@as(u32, 0xAB7F_A64D), hashString("DBFilesClient\\Spell.dbc", 0x200));
    // The hash is case-insensitive, like the client's lookups.
    try std.testing.expectEqual(hashString("foo.txt", 0x000), hashString("FOO.TXT", 0x000));
}

test "duplicate archive paths are rejected" {
    try std.testing.expectError(error.DuplicatePath, build(std.testing.allocator, &.{
        .{ .path = "foo.txt", .data = "one" },
        .{ .path = "FOO.TXT", .data = "two" },
    }));
}

test "archive paths are validated" {
    try std.testing.expect(isArchivePath("sum.txt"));
    try std.testing.expect(isArchivePath("DBFilesClient\\Spell.dbc"));
    try std.testing.expect(!isArchivePath(""));
    try std.testing.expect(!isArchivePath("\\foo"));
    try std.testing.expect(!isArchivePath("foo\\"));
    try std.testing.expect(!isArchivePath("foo//bar"));
    try std.testing.expect(!isArchivePath("foo/bar"));
    try std.testing.expect(!isArchivePath("../foo"));
    try std.testing.expect(!isArchivePath("a\\..\\b"));
    try std.testing.expect(!isArchivePath("C:foo"));
    try std.testing.expect(!isArchivePath("a\x00b"));
}

test "build produces a readable v1 archive" {
    const allocator = std.testing.allocator;
    const archive = try build(allocator, &.{
        .{ .path = "foo.txt", .data = "hello hello hello hello hello hello hello hello\n" },
        .{ .path = "patch-D.sum.txt", .data = "d41d8cd98f00b204e9800998ecf8427e  foo.txt\n" },
    });
    defer allocator.free(archive);

    try std.testing.expectEqual(@as(u32, 0x1A51_504D), std.mem.readInt(u32, archive[0..4], .little));
    try std.testing.expectEqual(@as(u32, header_size), std.mem.readInt(u32, archive[4..8], .little));
    try std.testing.expectEqual(@as(u32, @intCast(archive.len)), std.mem.readInt(u32, archive[8..12], .little));
    try std.testing.expectEqual(@as(u16, 0), std.mem.readInt(u16, archive[12..14], .little));

    const hash_table_offset = std.mem.readInt(u32, archive[16..20], .little);
    const block_table_offset = std.mem.readInt(u32, archive[20..24], .little);
    const hash_count = std.mem.readInt(u32, archive[24..28], .little);
    const block_count = std.mem.readInt(u32, archive[28..32], .little);
    try std.testing.expectEqual(@as(u32, 2), block_count);
    try std.testing.expectEqual(@as(u32, 4), hash_count);

    // Decrypt the tables in place in a copy and look the files up.
    const blocks = try allocator.dupe(u8, archive[block_table_offset..][0 .. block_count * table_entry_size]);
    defer allocator.free(blocks);
    decryptBlock(hashString("(block table)", 0x300), blocks);

    const hashes = try allocator.dupe(u8, archive[hash_table_offset..][0 .. hash_count * table_entry_size]);
    defer allocator.free(hashes);
    decryptBlock(hashString("(hash table)", 0x300), hashes);

    const contents = [_]struct { path: []const u8, data: []const u8 }{
        .{ .path = "foo.txt", .data = "hello hello hello hello hello hello hello hello\n" },
        .{ .path = "patch-D.sum.txt", .data = "d41d8cd98f00b204e9800998ecf8427e  foo.txt\n" },
    };
    for (contents) |expected| {
        const index = findHashEntry(hashes, hash_count, expected.path) orelse return error.TestUnexpectedResult;
        const block = blocks[index * table_entry_size ..][0..table_entry_size];
        const offset = std.mem.readInt(u32, block[0..4], .little);
        const packed_size = std.mem.readInt(u32, block[4..8], .little);
        const unpacked_size = std.mem.readInt(u32, block[8..12], .little);
        const flags = std.mem.readInt(u32, block[12..16], .little);

        try std.testing.expect(flags & flag_exists != 0);
        try std.testing.expect(flags & flag_single_unit != 0);
        try std.testing.expectEqual(@as(u32, @intCast(expected.data.len)), unpacked_size);

        const payload = archive[offset..][0..packed_size];
        if (flags & flag_compress != 0) {
            try std.testing.expectEqual(@as(u8, compression_zlib), payload[0]);
            const decompressed = try inflateZlib(allocator, payload[1..], unpacked_size);
            defer allocator.free(decompressed);
            try std.testing.expectEqualStrings(expected.data, decompressed);
        } else {
            try std.testing.expectEqual(unpacked_size, packed_size);
            try std.testing.expectEqualStrings(expected.data, payload);
        }
    }
}

fn findHashEntry(table: []const u8, hash_count: u32, path: []const u8) ?usize {
    const mask = hash_count - 1;
    var slot = hashString(path, 0x000) & mask;
    while (true) : (slot = (slot + 1) & mask) {
        const entry = table[slot * table_entry_size ..][0..table_entry_size];
        const name_a = std.mem.readInt(u32, entry[0..4], .little);
        const name_b = std.mem.readInt(u32, entry[4..8], .little);
        const block_index = std.mem.readInt(u32, entry[12..16], .little);
        if (block_index == hash_free) return null;
        if (name_a == hashString(path, 0x100) and name_b == hashString(path, 0x200)) return block_index;
    }
}

fn decryptBlock(seed_start: u32, bytes: []u8) void {
    var seed = seed_start;
    var seed2: u32 = 0xEEEE_EEEE;
    var i: usize = 0;
    while (i < bytes.len) : (i += 4) {
        const value = std.mem.readInt(u32, bytes[i..][0..4], .little);
        seed2 +%= crypt_table[0x400 + (seed & 0xFF)];
        const decrypted = value ^ (seed +% seed2);
        seed = ((~seed << 21) +% 0x1111_1111) | (seed >> 11);
        seed2 = decrypted +% seed2 +% (seed2 << 5) +% 3;
        std.mem.writeInt(u32, bytes[i..][0..4], decrypted, .little);
    }
}

fn inflateZlib(allocator: std.mem.Allocator, data: []const u8, size: u32) ![]u8 {
    var in = std.Io.Reader.fixed(data);
    const out = try allocator.alloc(u8, size);
    errdefer allocator.free(out);
    var window: [std.compress.flate.max_window_len]u8 = undefined;
    var decompressor = std.compress.flate.Decompress.init(&in, .zlib, &window);
    try decompressor.reader.readSliceAll(out);
    return out;
}
