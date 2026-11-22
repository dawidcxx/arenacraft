//! The layout solver.
//!
//! Port of Clay v0.14's sizing pass (`Clay__SizeContainersAlongAxis`), height
//! propagation and position pass (`Clay__CalculateFinalLayout`), without the
//! render command generation.
//!
//! The algorithm is unchanged, including the parts that look odd:
//!
//! * children are sized in a breadth-first pass along X, then text is wrapped,
//!   container heights are recomputed bottom-up, then the pass runs again for Y;
//! * leftover space is distributed with a "water filling" loop so siblings grow
//!   and shrink evenly until one of them hits its min/max;
//! * `percent` children are resolved against the parent's size minus padding
//!   and the child gaps on the layout axis.
//!
//! Deliberate deviations, all of which only remove features this port does not
//! have (clipping, z-index, transitions) or fix a Clay quirk:
//!
//! * no clip/scroll containers: a container therefore never ignores child
//!   minimum sizes;
//! * floating elements only attach to their parent or to the root (`attach_to`
//!   by element id needs the id hash map);
//! * the tree is walked in declaration order and roots are positioned in
//!   declaration order (no z-index sort);
//! * a `percent` axis is not re-clamped against min/max after height
//!   propagation, because Clay reads the min/max through the percent union
//!   member there, which is undefined behaviour in C.
//!
//! Everything is allocated from a per-frame arena owned by the engine, so a
//! caller only supplies an allocator once, at `init`.

const std = @import("std");

const types = @import("types.zig");
const text_mod = @import("text.zig");

const Allocator = std.mem.Allocator;

/// Handle for an element. Indices are stable for the lifetime of a frame;
/// `nodes()` is not, because elements are appended as the tree is built.
pub const NodeId = u32;

/// Clay's comparison tolerance for float sizes.
const epsilon: f32 = 0.01;

const TextData = struct {
    /// Borrowed from the caller; must stay alive until `compute` returns.
    content: []const u8 = "",
    preferred: types.Size = .{},
    min_width: f32 = 0,
    space_width: f32 = 0,
    contains_newlines: bool = false,
    words: []const text_mod.Word = &.{},
    lines: []const text_mod.Line = &.{},
};

pub const Node = struct {
    declaration: types.Declaration = .{},
    children: std.ArrayListUnmanaged(NodeId) = .empty,
    dimensions: types.Size = .{},
    min_dimensions: types.Size = .{},
    /// Absolute bounds inside the layout, filled in by `compute`.
    bounds: types.Rect = .{},
    is_text: bool = false,
    text_data: TextData = .{},

    pub fn lineCount(self: *const Node) usize {
        return self.text_data.lines.len;
    }
};

/// A layout root: the implicit root container, plus every floating element.
const Root = struct {
    id: NodeId,
    /// Element a floating root is attached to; null for the layout root.
    parent: ?NodeId = null,
};

const Frame = struct {
    id: NodeId,
    position: types.Point = .{},
};

pub const Layout = struct {
    arena: std.heap.ArenaAllocator,
    measure_fn: types.MeasureFn,
    measure_context: ?*anyopaque,

    elements: std.ArrayListUnmanaged(Node) = .empty,
    roots: std.ArrayListUnmanaged(Root) = .empty,
    open_stack: std.ArrayListUnmanaged(NodeId) = .empty,

    size: types.Size = .{},
    computed: bool = false,

    // Scratch buffers, reused across frames.
    bfs: std.ArrayListUnmanaged(NodeId) = .empty,
    resizable: std.ArrayListUnmanaged(NodeId) = .empty,
    queue: std.ArrayListUnmanaged(Frame) = .empty,
    stack: std.ArrayListUnmanaged(Frame) = .empty,
    visited: std.ArrayListUnmanaged(bool) = .empty,

    pub fn init(allocator: Allocator, measure_fn: types.MeasureFn, measure_context: ?*anyopaque) Layout {
        return .{
            .arena = std.heap.ArenaAllocator.init(allocator),
            .measure_fn = measure_fn,
            .measure_context = measure_context,
        };
    }

    pub fn deinit(self: *Layout) void {
        self.arena.deinit();
    }

    /// Start a frame with a root container covering `size`.
    pub fn begin(self: *Layout, size: types.Size) !void {
        _ = self.arena.reset(.retain_capacity);
        self.elements = .empty;
        self.roots = .empty;
        self.open_stack = .empty;
        self.bfs = .empty;
        self.resizable = .empty;
        self.queue = .empty;
        self.stack = .empty;
        self.visited = .empty;
        self.size = size;
        self.computed = false;

        const allocator = self.arena.allocator();
        const root = try self.createNode(.{
            .layout = .{ .sizing = .{
                .width = .{ .fixed = size.width },
                .height = .{ .fixed = size.height },
            } },
        });
        try self.roots.append(allocator, .{ .id = root });
        try self.open_stack.append(allocator, root);
    }

    /// Declare a container as a child of the currently open element.
    ///
    /// Returns `error.InvalidPercent` when a `percent` axis is outside `0..1`;
    /// Clay reports that through its error handler instead.
    pub fn open(self: *Layout, declaration: types.Declaration) !NodeId {
        std.debug.assert(!self.computed);
        std.debug.assert(self.open_stack.items.len > 0);
        try validateSizing(declaration.layout.sizing.width);
        try validateSizing(declaration.layout.sizing.height);

        const allocator = self.arena.allocator();
        const id = try self.createNode(declaration);

        if (declaration.floating.attach_to != .none) {
            const parent_id: NodeId = switch (declaration.floating.attach_to) {
                .parent => self.open_stack.items[self.open_stack.items.len - 1],
                .root => 0,
                .none => unreachable,
            };
            try self.roots.append(allocator, .{ .id = id, .parent = parent_id });
        }

        try self.open_stack.append(allocator, id);
        return id;
    }

    /// Close the currently open element. Cannot close the implicit root.
    pub fn close(self: *Layout) !void {
        std.debug.assert(self.open_stack.items.len > 1);

        const id = self.open_stack.items[self.open_stack.items.len - 1];
        self.open_stack.items.len -= 1;
        const parent_id = self.open_stack.items[self.open_stack.items.len - 1];

        self.finish(id);

        // Floating elements are positioned as their own roots; they are not
        // part of the parent's flow.
        if (self.elements.items[id].declaration.floating.attach_to == .none) {
            try self.elements.items[parent_id].children.append(self.arena.allocator(), id);
        }
    }

    /// Add a text element to the currently open element.
    pub fn text(self: *Layout, content: []const u8, config: types.Text) !NodeId {
        std.debug.assert(!self.computed);
        std.debug.assert(self.open_stack.items.len > 0);

        const allocator = self.arena.allocator();
        const measured = try text_mod.measure(allocator, self.measure_fn, self.measure_context, content, config);

        const id = try self.createNode(.{ .text = config });
        const line_height = if (config.line_height > 0) config.line_height else measured.preferred.height;

        const node = &self.elements.items[id];
        node.is_text = true;
        node.text_data = .{
            .content = content,
            .preferred = measured.preferred,
            .min_width = measured.min_width,
            .space_width = measured.space_width,
            .contains_newlines = measured.contains_newlines,
            .words = measured.words,
        };
        node.dimensions = .{ .width = measured.preferred.width, .height = line_height };
        node.min_dimensions = .{ .width = measured.min_width, .height = line_height };

        const parent_id = self.open_stack.items[self.open_stack.items.len - 1];
        try self.elements.items[parent_id].children.append(allocator, id);
        return id;
    }

    /// Run the solver. After this, every node has absolute `bounds`.
    pub fn compute(self: *Layout) !void {
        std.debug.assert(!self.computed);
        self.computed = true;

        // X sizes first, so text knows how wide it may be.
        try self.sizeAlongAxis(true);
        try self.wrapText();
        self.propagateHeights();
        try self.sizeAlongAxis(false);
        try self.positionRoots();
    }

    pub fn bounds(self: *const Layout, id: NodeId) types.Rect {
        return self.elements.items[id].bounds;
    }

    /// All elements in declaration order (index == `NodeId`).
    pub fn nodes(self: *const Layout) []const Node {
        return self.elements.items;
    }

    fn createNode(self: *Layout, declaration: types.Declaration) !NodeId {
        try self.elements.append(self.arena.allocator(), .{ .declaration = declaration });
        return @intCast(self.elements.items.len - 1);
    }

    /// Compute an element's intrinsic size once its children are known.
    fn finish(self: *Layout, id: NodeId) void {
        const node = &self.elements.items[id];
        const layout = node.declaration.layout;
        const padding = layout.padding;

        var dimensions: types.Size = .{};
        var minimum: types.Size = .{};

        const gaps = gapCount(node.children.items.len) * layout.child_gap;

        if (layout.direction == .left_to_right) {
            dimensions.width = padding.left + padding.right;
            minimum.width = dimensions.width;
            for (node.children.items) |child_id| {
                const child = self.elements.items[child_id];
                dimensions.width += child.dimensions.width;
                dimensions.height = @max(dimensions.height, child.dimensions.height + padding.top + padding.bottom);
                minimum.width += child.min_dimensions.width;
                minimum.height = @max(minimum.height, child.min_dimensions.height + padding.top + padding.bottom);
            }
            dimensions.width += gaps;
            minimum.width += gaps;
        } else {
            dimensions.height = padding.top + padding.bottom;
            minimum.height = dimensions.height;
            for (node.children.items) |child_id| {
                const child = self.elements.items[child_id];
                dimensions.height += child.dimensions.height;
                dimensions.width = @max(dimensions.width, child.dimensions.width + padding.left + padding.right);
                minimum.height += child.min_dimensions.height;
                minimum.width = @max(minimum.width, child.min_dimensions.width + padding.left + padding.right);
            }
            dimensions.height += gaps;
            minimum.height += gaps;
        }

        const width = closeAxis(layout.sizing.width, dimensions.width, minimum.width);
        node.dimensions.width = width.dimensions;
        node.min_dimensions.width = width.minimum;

        const height = closeAxis(layout.sizing.height, dimensions.height, minimum.height);
        node.dimensions.height = height.dimensions;
        node.min_dimensions.height = height.minimum;
    }

    /// One axis of Clay's `Clay__SizeContainersAlongAxis`.
    fn sizeAlongAxis(self: *Layout, x_axis: bool) !void {
        const allocator = self.arena.allocator();

        for (self.roots.items) |root| {
            self.bfs.clearRetainingCapacity();

            // A floating root takes its size from the element it attaches to.
            {
                const root_node = &self.elements.items[root.id];
                if (root.parent) |parent_id| {
                    const parent_size = axisOf(self.elements.items[parent_id].dimensions, x_axis);
                    const axis = axisSizing(root_node.declaration.layout.sizing, x_axis);
                    switch (axis) {
                        .grow => setAxisOf(&root_node.dimensions, x_axis, parent_size),
                        .percent => |percent| setAxisOf(&root_node.dimensions, x_axis, parent_size * percent),
                        else => {},
                    }
                }
                const axis = axisSizing(root_node.declaration.layout.sizing, x_axis);
                setAxisOf(&root_node.dimensions, x_axis, clampToSizing(axis, axisOf(root_node.dimensions, x_axis)));
            }

            try self.bfs.append(allocator, root.id);

            var cursor: usize = 0;
            while (cursor < self.bfs.items.len) : (cursor += 1) {
                const parent = &self.elements.items[self.bfs.items[cursor]];
                const parent_layout = parent.declaration.layout;
                const parent_size = axisOf(parent.dimensions, x_axis);
                const parent_padding = axisPadding(parent_layout.padding, x_axis);
                const along = if (x_axis)
                    parent_layout.direction == .left_to_right
                else
                    parent_layout.direction == .top_to_bottom;

                self.resizable.clearRetainingCapacity();
                var grow_count: u32 = 0;
                var inner_content: f32 = 0;
                var total_padding_and_gaps = parent_padding;
                var is_first = true;

                for (parent.children.items) |child_id| {
                    const child = &self.elements.items[child_id];
                    const child_sizing = elementSizing(child, x_axis);
                    const child_size = axisOf(child.dimensions, x_axis);

                    if (!child.is_text and child.children.items.len > 0) {
                        try self.bfs.append(allocator, child_id);
                    }

                    const resizable = switch (types.kind(child_sizing)) {
                        .percent, .fixed => false,
                        else => !child.is_text or child.declaration.text.wrap == .words,
                    };
                    if (resizable) try self.resizable.append(allocator, child_id);

                    if (along) {
                        if (types.kind(child_sizing) != .percent) inner_content += child_size;
                        if (types.kind(child_sizing) == .grow) grow_count += 1;
                        if (!is_first) {
                            inner_content += parent_layout.child_gap;
                            total_padding_and_gaps += parent_layout.child_gap;
                        }
                    } else {
                        inner_content = @max(child_size, inner_content);
                    }
                    is_first = false;
                }

                // Second pass: percentages need the total padding/gap first.
                for (parent.children.items) |child_id| {
                    const child = &self.elements.items[child_id];
                    const child_sizing = elementSizing(child, x_axis);
                    switch (child_sizing) {
                        .percent => |percent| {
                            const size = (parent_size - total_padding_and_gaps) * percent;
                            setAxisOf(&child.dimensions, x_axis, size);
                            if (along) inner_content += size;
                        },
                        else => {},
                    }
                }

                if (along) {
                    const to_distribute = parent_size - parent_padding - inner_content;
                    if (to_distribute < 0) {
                        self.shrink(to_distribute, x_axis);
                    } else if (to_distribute > 0 and grow_count > 0) {
                        self.grow(to_distribute, x_axis);
                    }
                } else {
                    for (self.resizable.items) |child_id| {
                        const child = &self.elements.items[child_id];
                        const child_sizing = elementSizing(child, x_axis);
                        const max_size = parent_size - parent_padding;
                        var size = axisOf(child.dimensions, x_axis);
                        if (types.kind(child_sizing) == .grow) {
                            size = @min(max_size, boundsOf(child_sizing).max);
                        }
                        size = @max(axisMinOf(child, x_axis), @min(size, max_size));
                        setAxisOf(&child.dimensions, x_axis, size);
                    }
                }
            }
        }
    }

    fn shrink(self: *Layout, to_distribute_in: f32, x_axis: bool) void {
        var to_distribute = to_distribute_in;

        while (to_distribute < -epsilon and self.resizable.items.len > 0) {
            var largest: f32 = 0;
            var second_largest: f32 = 0;
            var width_to_add = to_distribute;

            for (self.resizable.items) |child_id| {
                const size = axisOf(self.elements.items[child_id].dimensions, x_axis);
                if (floatEqual(size, largest)) continue;
                if (size > largest) {
                    second_largest = largest;
                    largest = size;
                }
                if (size < largest) {
                    second_largest = @max(second_largest, size);
                    width_to_add = second_largest - largest;
                }
            }

            width_to_add = @max(width_to_add, to_distribute / countOf(self.resizable.items.len));

            var index: usize = 0;
            while (index < self.resizable.items.len) {
                const child = &self.elements.items[self.resizable.items[index]];
                const size = axisPtr(&child.dimensions, x_axis);
                const minimum = axisMinOf(child, x_axis);
                const previous = size.*;

                if (floatEqual(size.*, largest)) {
                    size.* += width_to_add;
                    var removed = false;
                    if (size.* <= minimum) {
                        size.* = minimum;
                        _ = self.resizable.swapRemove(index);
                        removed = true;
                    }
                    // Clay accounts the actual delta even when the child was
                    // clamped to its minimum and dropped from the buffer.
                    to_distribute -= size.* - previous;
                    if (removed) continue;
                }
                index += 1;
            }
        }
    }

    fn grow(self: *Layout, to_distribute_in: f32, x_axis: bool) void {
        var to_distribute = to_distribute_in;

        // Only GROW siblings share the leftover space.
        var index: usize = 0;
        while (index < self.resizable.items.len) {
            const child = &self.elements.items[self.resizable.items[index]];
            if (types.kind(elementSizing(child, x_axis)) != .grow) {
                _ = self.resizable.swapRemove(index);
                continue;
            }
            index += 1;
        }

        while (to_distribute > epsilon and self.resizable.items.len > 0) {
            var smallest: f32 = std.math.floatMax(f32);
            var second_smallest: f32 = std.math.floatMax(f32);
            var width_to_add = to_distribute;

            for (self.resizable.items) |child_id| {
                const size = axisOf(self.elements.items[child_id].dimensions, x_axis);
                if (floatEqual(size, smallest)) continue;
                if (size < smallest) {
                    second_smallest = smallest;
                    smallest = size;
                }
                if (size > smallest) {
                    second_smallest = @min(second_smallest, size);
                    width_to_add = second_smallest - smallest;
                }
            }

            width_to_add = @min(width_to_add, to_distribute / countOf(self.resizable.items.len));

            index = 0;
            while (index < self.resizable.items.len) {
                const child = &self.elements.items[self.resizable.items[index]];
                const size = axisPtr(&child.dimensions, x_axis);
                const maximum = boundsOf(elementSizing(child, x_axis)).max;
                const previous = size.*;

                if (floatEqual(size.*, smallest)) {
                    size.* += width_to_add;
                    var removed = false;
                    if (size.* >= maximum) {
                        size.* = maximum;
                        _ = self.resizable.swapRemove(index);
                        removed = true;
                    }
                    to_distribute -= size.* - previous;
                    if (removed) continue;
                }
                index += 1;
            }
        }
    }

    fn wrapText(self: *Layout) !void {
        const allocator = self.arena.allocator();

        for (self.elements.items) |*node| {
            if (!node.is_text) continue;

            const config = node.declaration.text;
            const data = &node.text_data;
            const line_height = if (config.line_height > 0) config.line_height else data.preferred.height;

            if (config.wrap == .none) {
                node.dimensions.height = line_height;
                continue;
            }

            const measurement = text_mod.Measurement{
                .words = data.words,
                .preferred = data.preferred,
                .min_width = data.min_width,
                .space_width = data.space_width,
                .contains_newlines = data.contains_newlines,
            };

            const break_on_width = config.wrap == .words;
            data.lines = try text_mod.wrap(allocator, data.content, &measurement, config, node.dimensions.width, break_on_width);
            node.dimensions.height = line_height * countOf(data.lines.len);
        }
    }

    /// Recompute container heights bottom-up, so wrapped text and `grow`
    /// children affect their parents before the Y sizing pass.
    fn propagateHeights(self: *Layout) void {
        const allocator = self.arena.allocator();

        self.stack.clearRetainingCapacity();
        self.visited.clearRetainingCapacity();

        for (self.roots.items) |root| {
            self.stack.append(allocator, .{ .id = root.id }) catch return;
            self.visited.append(allocator, false) catch return;
        }

        while (self.stack.items.len > 0) {
            const depth = self.stack.items.len - 1;

            if (!self.visited.items[depth]) {
                self.visited.items[depth] = true;

                const id = self.stack.items[depth].id;
                const node = &self.elements.items[id];

                if (node.is_text or node.children.items.len == 0) {
                    self.stack.items.len -= 1;
                    self.visited.items.len -= 1;
                    continue;
                }

                for (node.children.items) |child_id| {
                    self.stack.append(allocator, .{ .id = child_id }) catch return;
                    self.visited.append(allocator, false) catch return;
                }
                continue;
            }

            const id = self.stack.items[depth].id;
            self.stack.items.len -= 1;
            self.visited.items.len -= 1;

            const node = &self.elements.items[id];
            const layout = node.declaration.layout;

            if (layout.direction == .left_to_right) {
                for (node.children.items) |child_id| {
                    const child = self.elements.items[child_id];
                    const needed = @max(
                        child.dimensions.height + layout.padding.top + layout.padding.bottom,
                        node.dimensions.height,
                    );
                    node.dimensions.height = clampToSizing(layout.sizing.height, needed);
                }
            } else {
                var content = layout.padding.top + layout.padding.bottom;
                for (node.children.items) |child_id| {
                    content += self.elements.items[child_id].dimensions.height;
                }
                content += gapCount(node.children.items.len) * layout.child_gap;
                node.dimensions.height = clampToSizing(layout.sizing.height, content);
            }
        }
    }

    fn positionRoots(self: *Layout) !void {
        const allocator = self.arena.allocator();

        for (self.roots.items) |root| {
            var origin: types.Point = .{};
            if (root.parent) |parent_id| {
                const root_node = self.elements.items[root.id];
                origin = attach(
                    self.elements.items[parent_id].bounds,
                    root_node.declaration.floating,
                    root_node.dimensions,
                );
            }
            try self.positionTree(allocator, root.id, origin);
        }
    }

    /// Positioning is top-down only, so it needs no second DFS pass.
    fn positionTree(self: *Layout, allocator: Allocator, root_id: NodeId, origin: types.Point) !void {
        self.queue.clearRetainingCapacity();
        try self.queue.append(allocator, .{ .id = root_id, .position = origin });

        var cursor: usize = 0;
        while (cursor < self.queue.items.len) : (cursor += 1) {
            const frame = self.queue.items[cursor];
            const node = &self.elements.items[frame.id];

            node.bounds = .{
                .x = frame.position.x,
                .y = frame.position.y,
                .width = node.dimensions.width,
                .height = node.dimensions.height,
            };

            if (node.is_text) continue;

            const layout = node.declaration.layout;
            const padding = layout.padding;
            var offset = types.Point{};

            // Alignment along the layout axis is applied once, to the group.
            if (layout.direction == .left_to_right) {
                var content_width: f32 = 0;
                for (node.children.items) |child_id| {
                    content_width += self.elements.items[child_id].dimensions.width;
                }
                content_width += gapCount(node.children.items.len) * layout.child_gap;

                var extra = node.dimensions.width - padding.left - padding.right - content_width;
                extra = switch (layout.child_alignment.x) {
                    .left => 0,
                    .center => extra / 2,
                    .right => extra,
                };
                offset.x = padding.left + @max(0, extra);
                offset.y = padding.top;
            } else {
                var content_height: f32 = 0;
                for (node.children.items) |child_id| {
                    content_height += self.elements.items[child_id].dimensions.height;
                }
                content_height += gapCount(node.children.items.len) * layout.child_gap;

                var extra = node.dimensions.height - padding.top - padding.bottom - content_height;
                extra = switch (layout.child_alignment.y) {
                    .top => 0,
                    .center => extra / 2,
                    .bottom => extra,
                };
                offset.y = padding.top + @max(0, extra);
                offset.x = padding.left;
            }

            for (node.children.items) |child_id| {
                const child = &self.elements.items[child_id];
                var child_position = types.Point{
                    .x = frame.position.x + offset.x,
                    .y = frame.position.y + offset.y,
                };

                if (layout.direction == .left_to_right) {
                    // Cross-axis alignment is per child.
                    const whitespace = node.dimensions.height - padding.top - padding.bottom - child.dimensions.height;
                    child_position.y += switch (layout.child_alignment.y) {
                        .top => 0,
                        .center => whitespace / 2,
                        .bottom => whitespace,
                    };
                    offset.x += child.dimensions.width + layout.child_gap;
                } else {
                    const whitespace = node.dimensions.width - padding.left - padding.right - child.dimensions.width;
                    child_position.x += switch (layout.child_alignment.x) {
                        .left => 0,
                        .center => whitespace / 2,
                        .right => whitespace,
                    };
                    offset.y += child.dimensions.height + layout.child_gap;
                }

                try self.queue.append(allocator, .{ .id = child_id, .position = child_position });
            }
        }
    }
};

fn validateSizing(axis: types.SizingAxis) !void {
    switch (axis) {
        .percent => |percent| {
            if (!(percent >= 0 and percent <= 1)) return error.InvalidPercent;
        },
        else => {},
    }
}

const ClosedAxis = struct { dimensions: f32, minimum: f32 };

/// Clay zeroes the size of a `percent` element when it closes, because the
/// real size is only known once the parent is sized.
fn closeAxis(sizing: types.SizingAxis, dimensions: f32, minimum: f32) ClosedAxis {
    return switch (sizing) {
        .percent => .{ .dimensions = 0, .minimum = minimum },
        else => .{
            .dimensions = clampToSizing(sizing, dimensions),
            .minimum = clampToSizing(sizing, minimum),
        },
    };
}

fn clampToSizing(sizing: types.SizingAxis, value: f32) f32 {
    return switch (sizing) {
        .fixed => |fixed| fixed,
        // A percent element is sized from its parent, not from its content.
        .percent => value,
        .fit, .grow => |bounds| @min(@max(value, bounds.min), bounds.max),
    };
}

fn boundsOf(sizing: types.SizingAxis) types.Bounds {
    return switch (sizing) {
        .fit => |bounds| bounds,
        .grow => |bounds| bounds,
        .fixed => |fixed| .{ .min = fixed, .max = fixed },
        .percent => .{},
    };
}

fn elementSizing(node: *const Node, x_axis: bool) types.SizingAxis {
    // Text is sized by its measurement, never by a declaration.
    if (node.is_text) return .{ .fit = .{} };
    return axisSizing(node.declaration.layout.sizing, x_axis);
}

fn axisSizing(sizing: types.Sizing, x_axis: bool) types.SizingAxis {
    return if (x_axis) sizing.width else sizing.height;
}

fn axisOf(size: types.Size, x_axis: bool) f32 {
    return if (x_axis) size.width else size.height;
}

fn setAxisOf(size: *types.Size, x_axis: bool, value: f32) void {
    if (x_axis) size.width = value else size.height = value;
}

fn axisPtr(size: *types.Size, x_axis: bool) *f32 {
    return if (x_axis) &size.width else &size.height;
}

fn axisMinOf(node: *const Node, x_axis: bool) f32 {
    return if (x_axis) node.min_dimensions.width else node.min_dimensions.height;
}

fn axisPadding(padding: types.Padding, x_axis: bool) f32 {
    return if (x_axis) padding.left + padding.right else padding.top + padding.bottom;
}

fn floatEqual(a: f32, b: f32) bool {
    const difference = a - b;
    return difference < epsilon and difference > -epsilon;
}

fn countOf(count: usize) f32 {
    return @floatFromInt(count);
}

/// Number of gaps between `count` children.
fn gapCount(count: usize) f32 {
    return if (count == 0) 0 else @floatFromInt(count - 1);
}

/// Where a floating element sits, given the box it is attached to.
fn attach(parent: types.Rect, floating: types.Floating, element: types.Size) types.Point {
    return .{
        .x = parent.x + fractionX(floating.parent.x) * parent.width -
            fractionX(floating.element.x) * element.width + floating.offset.x,
        .y = parent.y + fractionY(floating.parent.y) * parent.height -
            fractionY(floating.element.y) * element.height + floating.offset.y,
    };
}

fn fractionX(value: types.AlignX) f32 {
    return switch (value) {
        .left => 0,
        .center => 0.5,
        .right => 1,
    };
}

fn fractionY(value: types.AlignY) f32 {
    return switch (value) {
        .top => 0,
        .center => 0.5,
        .bottom => 1,
    };
}
