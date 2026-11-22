//! Text measurement and line breaking.
//!
//! Port of `Clay__MeasureTextCached` and the wrapping loop in
//! `Clay__CalculateFinalLayout`. Clay caches measurements across frames; this
//! implementation does not: the launcher lays out a handful of short strings
//! per resize, and an arena reset already makes per-frame allocation free.
//!
//! The measure callback always reports a string's natural (single line) size.
//! Splitting into words and deciding where lines break is this module's job,
//! which is why layout only needs one callback from the embedder.

const std = @import("std");

const types = @import("types.zig");

/// One measured word. Lines break between words, so a word is the smallest
/// unit the layout needs to measure.
pub const Word = struct {
    /// Byte offset into the text element's content.
    offset: u32,
    len: u32,
    /// Width including one trailing space (if the word was followed by one).
    width: f32,
};

/// One laid-out line, as byte offsets into the text element's content.
pub const Line = struct {
    offset: u32,
    len: u32,
    width: f32,
    height: f32,
};

pub const Measurement = struct {
    words: []const Word = &.{},
    /// Size of the text if it never wrapped.
    preferred: types.Size = .{},
    /// Width of the longest word: the narrowest the text can be.
    min_width: f32 = 0,
    space_width: f32 = 0,
    contains_newlines: bool = false,
};

/// Split `content` into words and measure them.
///
/// Words keep their trailing space so a line's width can be computed by adding
/// word widths; `min_width` uses the word alone, without that space, matching
/// Clay.
pub fn measure(
    allocator: std.mem.Allocator,
    measure_fn: types.MeasureFn,
    context: ?*anyopaque,
    content: []const u8,
    config: types.Text,
) !Measurement {
    var words: std.ArrayListUnmanaged(Word) = .empty;

    const space_width = measure_fn(context, " ", config.measure_key).width;
    var result: Measurement = .{ .space_width = space_width };

    var line_width: f32 = 0;
    var measured_width: f32 = 0;
    var measured_height: f32 = 0;
    var start: usize = 0;
    var end: usize = 0;

    while (end < content.len) : (end += 1) {
        const current = content[end];
        if (current != ' ' and current != '\n') continue;

        const len = end - start;
        var dimensions: types.Size = .{};
        if (len > 0) dimensions = measure_fn(context, content[start..end], config.measure_key);

        result.min_width = @max(dimensions.width, result.min_width);
        measured_height = @max(measured_height, dimensions.height);

        if (current == ' ') {
            dimensions.width += space_width;
            try words.append(allocator, .{
                .offset = @intCast(start),
                .len = @intCast(len + 1),
                .width = dimensions.width,
            });
            line_width += dimensions.width;
        }

        if (current == '\n') {
            if (len > 0) {
                try words.append(allocator, .{
                    .offset = @intCast(start),
                    .len = @intCast(len),
                    .width = dimensions.width,
                });
            }
            // A zero-length word is a forced break.
            try words.append(allocator, .{ .offset = @intCast(end + 1), .len = 0, .width = 0 });
            line_width += dimensions.width;
            measured_width = @max(line_width, measured_width);
            result.contains_newlines = true;
            line_width = 0;
        }

        start = end + 1;
    }

    if (end > start) {
        const dimensions = measure_fn(context, content[start..end], config.measure_key);
        try words.append(allocator, .{
            .offset = @intCast(start),
            .len = @intCast(end - start),
            .width = dimensions.width,
        });
        line_width += dimensions.width;
        measured_height = @max(measured_height, dimensions.height);
        result.min_width = @max(dimensions.width, result.min_width);
    }

    result.words = try words.toOwnedSlice(allocator);
    result.preferred = .{
        .width = @max(line_width, measured_width) - config.letter_spacing,
        .height = measured_height,
    };
    return result;
}

/// Break `content` into lines.
///
/// With `break_on_width` the greedy word wrap matches Clay's: a single word
/// wider than the line is emitted on its own line and overflows, and a trailing
/// space does not count towards the line's width. Without it only explicit
/// newlines break, which is what Clay documents for `CLAY_TEXT_WRAP_NEWLINES`
/// (Clay actually still breaks on width there, because the wrap loop does not
/// check the mode).
pub fn wrap(
    allocator: std.mem.Allocator,
    content: []const u8,
    measurement: *const Measurement,
    config: types.Text,
    max_width: f32,
    break_on_width: bool,
) ![]const Line {
    var lines: std.ArrayListUnmanaged(Line) = .empty;

    const line_height = if (config.line_height > 0) config.line_height else measurement.preferred.height;

    if (!measurement.contains_newlines and (!break_on_width or measurement.preferred.width <= max_width)) {
        try lines.append(allocator, .{
            .offset = 0,
            .len = @intCast(content.len),
            .width = measurement.preferred.width,
            .height = line_height,
        });
        return lines.toOwnedSlice(allocator);
    }

    var line_width: f32 = 0;
    var line_len: usize = 0;
    var line_start: usize = 0;
    var word_index: usize = 0;

    while (word_index < measurement.words.len) {
        const word = measurement.words[word_index];

        // A single word that cannot fit starts its own (overflowing) line.
        if (break_on_width and line_len == 0 and line_width + word.width > max_width) {
            try lines.append(allocator, .{
                .offset = word.offset,
                .len = word.len,
                .width = word.width,
                .height = line_height,
            });
            line_start = word.offset + word.len;
            word_index += 1;
            continue;
        }

        if (word.len == 0 or (break_on_width and line_width + word.width > max_width)) {
            // Finish the current line. A trailing space is not part of it.
            var line_end = line_start + line_len;
            var width = line_width;
            if (line_len > 0 and content[line_end - 1] == ' ') {
                line_end -= 1;
                width -= measurement.space_width;
            }
            try lines.append(allocator, .{
                .offset = @intCast(line_start),
                .len = @intCast(line_end - line_start),
                .width = width,
                .height = line_height,
            });

            // A forced break consumes the break word; otherwise the word that
            // did not fit starts the next line.
            if (line_len == 0 or word.len == 0) word_index += 1;

            line_width = 0;
            line_len = 0;
            line_start = word.offset;
            continue;
        }

        line_width += word.width + config.letter_spacing;
        line_len += word.len;
        word_index += 1;
    }

    if (line_len > 0) {
        try lines.append(allocator, .{
            .offset = @intCast(line_start),
            .len = @intCast(line_len),
            .width = line_width - config.letter_spacing,
            .height = line_height,
        });
    }

    return lines.toOwnedSlice(allocator);
}
