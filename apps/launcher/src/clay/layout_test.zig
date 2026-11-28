//! Tests for the Clay layout port.
//!
//! The measure function is intentionally trivial (10px per byte, 20px line
//! height) so every expected size in this file can be computed by hand.

const std = @import("std");

const clay = @import("root.zig");
const types = clay.types;

/// 10px per byte, 20px line height.
fn measure(_: ?*anyopaque, content: []const u8, _: u32) types.Size {
    return .{ .width = @as(f32, @floatFromInt(content.len)) * 10, .height = 20 };
}

fn fixed(value: f32) types.SizingAxis {
    return .{ .fixed = value };
}

fn growAxis() types.SizingAxis {
    return .{ .grow = .{} };
}

fn percent(value: f32) types.SizingAxis {
    return .{ .percent = value };
}

/// Fixed on both axes.
fn boxed(width: f32, height: f32) types.Sizing {
    return .{ .width = fixed(width), .height = fixed(height) };
}

fn expectRect(expected: types.Rect, actual: types.Rect) !void {
    try std.testing.expectApproxEqAbs(expected.x, actual.x, 0.01);
    try std.testing.expectApproxEqAbs(expected.y, actual.y, 0.01);
    try std.testing.expectApproxEqAbs(expected.width, actual.width, 0.01);
    try std.testing.expectApproxEqAbs(expected.height, actual.height, 0.01);
}

fn expectSize(expected: types.Size, actual: types.Size) !void {
    try std.testing.expectApproxEqAbs(expected.width, actual.width, 0.01);
    try std.testing.expectApproxEqAbs(expected.height, actual.height, 0.01);
}

test "fit container wraps its children plus padding and gaps" {
    var layout = clay.Layout.init(std.testing.allocator, measure, null);
    defer layout.deinit();

    try layout.begin(.{ .width = 100, .height = 100 });
    _ = try layout.open(.{ .layout = .{
        .padding = .all(10),
        .child_gap = 5,
    } });
    _ = try layout.open(.{ .layout = .{ .sizing = boxed(20, 20) } });
    try layout.close();
    _ = try layout.open(.{ .layout = .{ .sizing = boxed(30, 20) } });
    try layout.close();
    try layout.close();
    try layout.compute();

    const nodes = layout.nodes();
    try std.testing.expectEqual(@as(usize, 4), nodes.len);

    // 20 padding + 20 + 5 gap + 30, height 20 + 20 padding.
    try expectRect(.{ .x = 0, .y = 0, .width = 75, .height = 40 }, nodes[1].bounds);
    try expectRect(.{ .x = 10, .y = 10, .width = 20, .height = 20 }, nodes[2].bounds);
    try expectRect(.{ .x = 35, .y = 10, .width = 30, .height = 20 }, nodes[3].bounds);
}

test "grow children share leftover space equally" {
    var layout = clay.Layout.init(std.testing.allocator, measure, null);
    defer layout.deinit();

    try layout.begin(.{ .width = 100, .height = 100 });
    _ = try layout.open(.{ .layout = .{ .sizing = .{ .width = growAxis(), .height = fixed(10) } } });
    try layout.close();
    _ = try layout.open(.{ .layout = .{ .sizing = .{ .width = growAxis(), .height = fixed(10) } } });
    try layout.close();
    try layout.compute();

    const nodes = layout.nodes();
    try expectRect(.{ .x = 0, .y = 0, .width = 50, .height = 10 }, nodes[1].bounds);
    try expectRect(.{ .x = 50, .y = 0, .width = 50, .height = 10 }, nodes[2].bounds);
}

test "grow stops at max and the rest goes to the other sibling" {
    var layout = clay.Layout.init(std.testing.allocator, measure, null);
    defer layout.deinit();

    try layout.begin(.{ .width = 100, .height = 100 });
    _ = try layout.open(.{ .layout = .{ .sizing = .{
        .width = .{ .grow = .{ .max = 20 } },
        .height = fixed(10),
    } } });
    try layout.close();
    _ = try layout.open(.{ .layout = .{ .sizing = .{ .width = growAxis(), .height = fixed(10) } } });
    try layout.close();
    try layout.compute();

    const nodes = layout.nodes();
    try std.testing.expectApproxEqAbs(@as(f32, 20), nodes[1].dimensions.width, 0.01);
    try std.testing.expectApproxEqAbs(@as(f32, 80), nodes[2].dimensions.width, 0.01);
}

test "percent children resolve against parent minus padding and gaps" {
    var layout = clay.Layout.init(std.testing.allocator, measure, null);
    defer layout.deinit();

    try layout.begin(.{ .width = 200, .height = 100 });
    _ = try layout.open(.{
        .layout = .{
            // Percent children need a parent with a definite size; a fit parent
            // has none to give them.
            .sizing = .{ .width = growAxis() },
            .padding = .all(10),
            .child_gap = 10,
        },
    });
    _ = try layout.open(.{ .layout = .{ .sizing = .{ .width = percent(0.5), .height = fixed(10) } } });
    try layout.close();
    _ = try layout.open(.{ .layout = .{ .sizing = .{ .width = percent(0.5), .height = fixed(10) } } });
    try layout.close();
    try layout.close();
    try layout.compute();

    const nodes = layout.nodes();
    // (200 - 20 padding - 10 gap) * 0.5
    const width: f32 = 85;
    try expectSize(.{ .width = width, .height = 10 }, nodes[2].dimensions);
    try expectRect(.{ .x = 10, .y = 10, .width = width, .height = 10 }, nodes[2].bounds);
    try expectRect(.{ .x = 105, .y = 10, .width = width, .height = 10 }, nodes[3].bounds);
}

test "out of range percent is rejected" {
    var layout = clay.Layout.init(std.testing.allocator, measure, null);
    defer layout.deinit();

    try layout.begin(.{ .width = 100, .height = 100 });
    try std.testing.expectError(
        error.InvalidPercent,
        layout.open(.{ .layout = .{ .sizing = .{ .width = percent(1.5) } } }),
    );
    try std.testing.expectError(
        error.InvalidPercent,
        layout.open(.{ .layout = .{ .sizing = .{ .height = percent(-0.1) } } }),
    );
}

test "column stacks children with gap and centres them on the cross axis" {
    var layout = clay.Layout.init(std.testing.allocator, measure, null);
    defer layout.deinit();

    try layout.begin(.{ .width = 100, .height = 200 });
    _ = try layout.open(.{ .layout = .{
        .sizing = .{ .width = growAxis(), .height = growAxis() },
        .direction = .top_to_bottom,
        .child_gap = 10,
        .child_alignment = .{ .x = .center },
    } });
    _ = try layout.open(.{ .layout = .{ .sizing = boxed(20, 30) } });
    try layout.close();
    _ = try layout.open(.{ .layout = .{ .sizing = boxed(40, 30) } });
    try layout.close();
    try layout.close();
    try layout.compute();

    const nodes = layout.nodes();
    try expectRect(.{ .x = 0, .y = 0, .width = 100, .height = 200 }, nodes[1].bounds);
    try expectRect(.{ .x = 40, .y = 0, .width = 20, .height = 30 }, nodes[2].bounds);
    try expectRect(.{ .x = 30, .y = 40, .width = 40, .height = 30 }, nodes[3].bounds);
}

test "grow stretches a child across the column" {
    var layout = clay.Layout.init(std.testing.allocator, measure, null);
    defer layout.deinit();

    try layout.begin(.{ .width = 100, .height = 50 });
    _ = try layout.open(.{ .layout = .{
        .sizing = .{ .width = growAxis(), .height = growAxis() },
        .direction = .top_to_bottom,
    } });
    _ = try layout.open(.{ .layout = .{ .sizing = .{ .width = growAxis(), .height = fixed(10) } } });
    try layout.close();
    try layout.close();
    try layout.compute();

    try expectSize(.{ .width = 100, .height = 10 }, layout.nodes()[2].dimensions);
}

test "text is measured and placed inside padding" {
    var layout = clay.Layout.init(std.testing.allocator, measure, null);
    defer layout.deinit();

    try layout.begin(.{ .width = 200, .height = 100 });
    _ = try layout.open(.{ .layout = .{ .padding = .all(10) } });
    _ = try layout.text("hello", .{});
    try layout.close();
    try layout.compute();

    const nodes = layout.nodes();
    try expectRect(.{ .x = 0, .y = 0, .width = 70, .height = 40 }, nodes[1].bounds);
    try expectRect(.{ .x = 10, .y = 10, .width = 50, .height = 20 }, nodes[2].bounds);
}

test "words wrap and the text grows in height" {
    var layout = clay.Layout.init(std.testing.allocator, measure, null);
    defer layout.deinit();

    try layout.begin(.{ .width = 50, .height = 100 });
    _ = try layout.text("aaaa bb", .{});
    try layout.compute();

    const node = &layout.nodes()[1];
    // Shrinks from its natural 70px to the 50px available, but not below the
    // 40px longest word.
    try expectRect(.{ .x = 0, .y = 0, .width = 50, .height = 40 }, node.bounds);
    try std.testing.expectEqual(@as(usize, 2), node.lineCount());
    try std.testing.expectEqual(@as(u32, 4), node.text_data.lines[0].len);
    try std.testing.expectApproxEqAbs(@as(f32, 40), node.text_data.lines[0].width, 0.01);
    try std.testing.expectEqual(@as(u32, 2), node.text_data.lines[1].len);
    try std.testing.expectApproxEqAbs(@as(f32, 20), node.text_data.lines[1].width, 0.01);
}

test "explicit newlines break lines" {
    var layout = clay.Layout.init(std.testing.allocator, measure, null);
    defer layout.deinit();

    try layout.begin(.{ .width = 100, .height = 100 });
    _ = try layout.text("ab\ncd", .{});
    try layout.compute();

    const node = &layout.nodes()[1];
    try expectSize(.{ .width = 20, .height = 40 }, node.dimensions);
    try std.testing.expectEqual(@as(usize, 2), node.lineCount());
    try std.testing.expectEqual(@as(u32, 0), node.text_data.lines[0].offset);
    try std.testing.expectEqual(@as(u32, 3), node.text_data.lines[1].offset);
}

test "wrap mode none keeps a single overflowing line" {
    var layout = clay.Layout.init(std.testing.allocator, measure, null);
    defer layout.deinit();

    try layout.begin(.{ .width = 50, .height = 100 });
    _ = try layout.text("aaaa bb", .{ .wrap = .none });
    try layout.compute();

    // Text that does not wrap is not resizable, so it overflows the root.
    try expectSize(.{ .width = 70, .height = 20 }, layout.nodes()[1].dimensions);
}

test "wrap mode newlines only breaks on newlines" {
    var layout = clay.Layout.init(std.testing.allocator, measure, null);
    defer layout.deinit();

    try layout.begin(.{ .width = 50, .height = 100 });
    _ = try layout.text("aaaa bb", .{ .wrap = .newlines });
    try layout.compute();

    try expectSize(.{ .width = 70, .height = 20 }, layout.nodes()[1].dimensions);
}

test "floating element is positioned from its parent and leaves the flow" {
    var layout = clay.Layout.init(std.testing.allocator, measure, null);
    defer layout.deinit();

    try layout.begin(.{ .width = 100, .height = 100 });
    _ = try layout.open(.{ .layout = .{ .sizing = boxed(40, 40) } });
    _ = try layout.open(.{
        .layout = .{ .sizing = boxed(20, 20) },
        .floating = .{
            .attach_to = .parent,
            .parent = .{ .x = .right, .y = .bottom },
            .element = .{ .x = .right, .y = .bottom },
            .offset = .{ .x = -5, .y = -5 },
        },
    });
    try layout.close();
    try layout.close();
    try layout.compute();

    const nodes = layout.nodes();
    // The parent keeps its fixed size: the floating child is not in the flow.
    try expectRect(.{ .x = 0, .y = 0, .width = 40, .height = 40 }, nodes[1].bounds);
    try expectRect(.{ .x = 15, .y = 15, .width = 20, .height = 20 }, nodes[2].bounds);
}

test "floating element can attach to the layout root" {
    var layout = clay.Layout.init(std.testing.allocator, measure, null);
    defer layout.deinit();

    try layout.begin(.{ .width = 100, .height = 100 });
    _ = try layout.open(.{
        .layout = .{ .sizing = boxed(20, 20) },
        .floating = .{
            .attach_to = .root,
            .parent = .{ .x = .right, .y = .bottom },
            .element = .{ .x = .right, .y = .bottom },
        },
    });
    try layout.close();
    try layout.compute();

    try expectRect(.{ .x = 80, .y = 80, .width = 20, .height = 20 }, layout.nodes()[1].bounds);
}

test "nested padding accumulates into absolute offsets" {
    var layout = clay.Layout.init(std.testing.allocator, measure, null);
    defer layout.deinit();

    try layout.begin(.{ .width = 100, .height = 100 });
    _ = try layout.open(.{ .layout = .{ .padding = .all(10) } });
    _ = try layout.open(.{ .layout = .{ .padding = .all(10) } });
    _ = try layout.open(.{ .layout = .{ .sizing = boxed(10, 10) } });
    try layout.close();
    try layout.close();
    try layout.close();
    try layout.compute();

    try expectRect(.{ .x = 20, .y = 20, .width = 10, .height = 10 }, layout.nodes()[3].bounds);
}

test "text shrink respects the longest word" {
    var layout = clay.Layout.init(std.testing.allocator, measure, null);
    defer layout.deinit();

    try layout.begin(.{ .width = 20, .height = 100 });
    _ = try layout.text("aaaa bb", .{});
    try layout.compute();

    // 20px cannot fit the 40px word, so the text stops at its minimum width.
    try std.testing.expectApproxEqAbs(@as(f32, 40), layout.nodes()[1].dimensions.width, 0.01);
}

test "begin resets the previous frame" {
    var layout = clay.Layout.init(std.testing.allocator, measure, null);
    defer layout.deinit();

    try layout.begin(.{ .width = 100, .height = 100 });
    _ = try layout.open(.{ .layout = .{ .sizing = boxed(10, 10) } });
    try layout.close();
    try layout.compute();
    try std.testing.expectEqual(@as(usize, 2), layout.nodes().len);

    try layout.begin(.{ .width = 100, .height = 100 });
    try std.testing.expectEqual(@as(usize, 1), layout.nodes().len);
}

test "launcher home column" {
    var layout = clay.Layout.init(std.testing.allocator, measure, null);
    defer layout.deinit();

    // The launcher's default client size.
    try layout.begin(.{ .width = 360, .height = 520 });
    _ = try layout.open(.{ .layout = .{
        .sizing = .{ .width = growAxis(), .height = growAxis() },
        .padding = .all(20),
        .child_gap = 14,
        .direction = .top_to_bottom,
    } });

    // Header, centred over the column.
    const header = try layout.open(.{ .layout = .{
        .sizing = .{ .width = growAxis() },
        .child_gap = 2,
        .direction = .top_to_bottom,
        .child_alignment = .{ .x = .center },
    } });
    const title = try layout.text("Arenacraft Launcher", .{ .wrap = .none });
    const tagline = try layout.text("WHERE THE CHALLENGE BEGINS", .{ .wrap = .none });
    try layout.close();

    // Spacer that pushes the actions to the bottom.
    const spacer = try layout.open(.{ .layout = .{ .sizing = .{ .height = growAxis() } } });
    try layout.close();

    // Three full-width, equally tall actions.
    const actions = try layout.open(.{ .layout = .{
        .sizing = .{ .width = growAxis() },
        .child_gap = 10,
        .direction = .top_to_bottom,
    } });
    const launch = try layout.open(.{ .layout = .{ .sizing = .{ .width = growAxis(), .height = fixed(48) } } });
    try layout.close();
    const change_realm = try layout.open(.{ .layout = .{ .sizing = .{ .width = growAxis(), .height = fixed(48) } } });
    try layout.close();
    const quit = try layout.open(.{ .layout = .{ .sizing = .{ .width = growAxis(), .height = fixed(48) } } });
    try layout.close();
    try layout.close();

    // Footer: the realm, small and left aligned on the bottom edge.
    const footer = try layout.open(.{ .layout = .{
        .sizing = .{ .width = growAxis() },
        .child_gap = 4,
        .child_alignment = .{ .y = .center },
    } });
    const realm_caption = try layout.text("realm:", .{ .wrap = .none });
    const realm_host = try layout.text("logon.arenacraft.fun", .{ .wrap = .none });
    try layout.close();

    try layout.close();
    try layout.compute();

    // Header wraps the centred title and tagline.
    try expectRect(.{ .x = 20, .y = 20, .width = 320, .height = 42 }, layout.bounds(header));
    try expectRect(.{ .x = 85, .y = 20, .width = 190, .height = 20 }, layout.bounds(title));
    try expectRect(.{ .x = 50, .y = 42, .width = 260, .height = 20 }, layout.bounds(tagline));

    // The spacer absorbs the 212px between the header and the actions.
    try expectRect(.{ .x = 20, .y = 76, .width = 0, .height = 212 }, layout.bounds(spacer));

    // The actions stretch across the column and end above the footer.
    try expectRect(.{ .x = 20, .y = 302, .width = 320, .height = 164 }, layout.bounds(actions));
    try expectRect(.{ .x = 20, .y = 302, .width = 320, .height = 48 }, layout.bounds(launch));
    try expectRect(.{ .x = 20, .y = 360, .width = 320, .height = 48 }, layout.bounds(change_realm));
    try expectRect(.{ .x = 20, .y = 418, .width = 320, .height = 48 }, layout.bounds(quit));

    // The realm sits on the bottom-left padding edge.
    try expectRect(.{ .x = 20, .y = 480, .width = 320, .height = 20 }, layout.bounds(footer));
    try expectRect(.{ .x = 20, .y = 480, .width = 60, .height = 20 }, layout.bounds(realm_caption));
    try expectRect(.{ .x = 84, .y = 480, .width = 200, .height = 20 }, layout.bounds(realm_host));
}
