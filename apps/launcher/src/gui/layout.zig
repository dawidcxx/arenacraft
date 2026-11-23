//! Layout vocabulary exposed by `gui`.
//!
//! These types mirror the `clay` module one to one so launcher code never
//! imports the engine. The conversions at the bottom are exhaustive switches:
//! adding an option to the engine forces a compile error here until it is
//! mapped, which keeps the mirror honest instead of silently dropping values.
//!
//! Sizes are in pixels as `f32` because `percent` and `grow` need fractions.
//! Widgets still receive whole pixels: `toRect` rounds at the boundary.

const std = @import("std");
const clay = @import("clay");

const geometry = @import("geometry.zig");

/// Bounds for `fit` and `grow` sizing.
pub const MinMax = struct {
    min: f32 = 0,
    /// Unbounded by default.
    max: f32 = std.math.inf(f32),
};

/// How a widget takes up space along one axis of its container.
pub const SizingAxis = union(enum) {
    /// Wrap tightly around content.
    fit: MinMax,
    /// Share the container's leftover space with other `grow` siblings.
    grow: MinMax,
    /// Fraction (0..1) of the container minus padding and gaps.
    percent: f32,
    /// Exact size.
    fixed: f32,
};

pub const Sizing = struct {
    width: SizingAxis = .{ .fit = .{} },
    height: SizingAxis = .{ .fit = .{} },

    pub const grow_both: Sizing = .{ .width = .{ .grow = .{} }, .height = .{ .grow = .{} } };
    pub const grow_width: Sizing = .{ .width = .{ .grow = .{} } };
    pub const grow_height: Sizing = .{ .height = .{ .grow = .{} } };

    pub fn fixed(width: f32, height: f32) Sizing {
        return .{ .width = .{ .fixed = width }, .height = .{ .fixed = height } };
    }

    /// Fixed width, grows to fill the remaining height.
    pub fn fixedWidth(width: f32) Sizing {
        return .{ .width = .{ .fixed = width }, .height = .{ .grow = .{} } };
    }

    /// Grows to fill the width, fixed height.
    pub fn fixedHeight(height: f32) Sizing {
        return .{ .width = .{ .grow = .{} }, .height = .{ .fixed = height } };
    }
};

pub const Padding = struct {
    left: f32 = 0,
    right: f32 = 0,
    top: f32 = 0,
    bottom: f32 = 0,

    pub fn all(value: f32) Padding {
        return .{ .left = value, .right = value, .top = value, .bottom = value };
    }

    pub fn symmetric(horizontal: f32, vertical: f32) Padding {
        return .{ .left = horizontal, .right = horizontal, .top = vertical, .bottom = vertical };
    }
};

pub const Direction = enum {
    /// Children are laid out left to right (a row).
    left_to_right,
    /// Children are laid out top to bottom (a column).
    top_to_bottom,
};

pub const AlignX = enum { left, center, right };
pub const AlignY = enum { top, center, bottom };

pub const Alignment = struct {
    x: AlignX = .left,
    y: AlignY = .top,
};

/// Layout of a container added with `Window.container`.
pub const Container = struct {
    sizing: Sizing = .{},
    padding: Padding = .{},
    /// Gap between adjacent children along `direction`.
    gap: f32 = 0,
    direction: Direction = .left_to_right,
    alignment: Alignment = .{},
};

pub const WrapMode = enum {
    /// Break lines on spaces and newlines.
    words,
    /// Break lines only on newlines.
    newlines,
    /// Never break lines.
    none,
};

/// Convert a size to whole pixels, clamping non-finite values to zero.
pub fn roundToInt(value: f32) i32 {
    if (!std.math.isFinite(value)) return 0;
    return @intFromFloat(@round(value));
}

/// Convert engine bounds to the widget placement type.
pub fn toRect(bounds: clay.Rect) geometry.Rect {
    return .{
        .x = roundToInt(bounds.x),
        .y = roundToInt(bounds.y),
        .width = @max(0, roundToInt(bounds.width)),
        .height = @max(0, roundToInt(bounds.height)),
    };
}

pub fn toClayBounds(bounds: MinMax) clay.Bounds {
    return .{ .min = bounds.min, .max = bounds.max };
}

pub fn toClayAxis(axis: SizingAxis) clay.SizingAxis {
    return switch (axis) {
        .fit => |bounds| .{ .fit = toClayBounds(bounds) },
        .grow => |bounds| .{ .grow = toClayBounds(bounds) },
        .percent => |percent| .{ .percent = percent },
        .fixed => |size| .{ .fixed = size },
    };
}

pub fn toClaySizing(sizing: Sizing) clay.Sizing {
    return .{
        .width = toClayAxis(sizing.width),
        .height = toClayAxis(sizing.height),
    };
}

pub fn toClayPadding(padding: Padding) clay.Padding {
    return .{
        .left = padding.left,
        .right = padding.right,
        .top = padding.top,
        .bottom = padding.bottom,
    };
}

pub fn toClayDirection(direction: Direction) clay.Direction {
    return switch (direction) {
        .left_to_right => .left_to_right,
        .top_to_bottom => .top_to_bottom,
    };
}

pub fn toClayAlignX(value: AlignX) clay.AlignX {
    return switch (value) {
        .left => .left,
        .center => .center,
        .right => .right,
    };
}

pub fn toClayAlignY(value: AlignY) clay.AlignY {
    return switch (value) {
        .top => .top,
        .center => .center,
        .bottom => .bottom,
    };
}

pub fn toClayContainer(container: Container) clay.types.Layout {
    return .{
        .sizing = toClaySizing(container.sizing),
        .padding = toClayPadding(container.padding),
        .child_gap = container.gap,
        .direction = toClayDirection(container.direction),
        .child_alignment = .{
            .x = toClayAlignX(container.alignment.x),
            .y = toClayAlignY(container.alignment.y),
        },
    };
}

pub fn toClayWrap(mode: WrapMode) clay.WrapMode {
    return switch (mode) {
        .words => .words,
        .newlines => .newlines,
        .none => .none,
    };
}
