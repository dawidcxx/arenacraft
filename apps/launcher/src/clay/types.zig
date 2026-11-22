//! Layout configuration vocabulary.
//!
//! A direct port of Clay's layout configuration (v0.14), minus everything that
//! only exists to generate draw commands: colors, corner radius, borders,
//! clipping/scrolling, transitions, pointer state, image/custom elements and
//! the debug view. What is left is the part that decides *where boxes go*.
//!
//! Names follow Clay, translated to Zig conventions (`childGap` ->
//! `child_gap`). Two deliberate deviations, both documented where they matter:
//!
//! * `SizingAxis` is a tagged union instead of a struct with an enum tag and an
//!   overlapping union, so an axis cannot carry a value its kind ignores.
//! * `Bounds.max` defaults to infinity instead of Clay treating `max <= 0` as
//!   unbounded, which makes a literal `0` unusable as a maximum.
//!
//! All sizes are in pixels as `f32`: the layout can be scaled (DPI) without
//! integer rounding, and the consumer rounds only when it places a widget.

const std = @import("std");

/// Offset, in pixels.
pub const Point = struct {
    x: f32 = 0,
    y: f32 = 0,
};

/// Dimensions, in pixels.
pub const Size = struct {
    width: f32 = 0,
    height: f32 = 0,
};

/// Position and size, in pixels.
pub const Rect = struct {
    x: f32 = 0,
    y: f32 = 0,
    width: f32 = 0,
    height: f32 = 0,
};

/// Bounds for `fit` and `grow` sizing.
pub const Bounds = struct {
    min: f32 = 0,
    /// Unbounded by default.
    max: f32 = std.math.inf(f32),
};

/// How an element takes up space along one axis of its parent.
pub const SizingAxis = union(enum) {
    /// Wrap tightly around content (children, text, padding).
    fit: Bounds,
    /// Fill the parent, sharing the leftover space with other `grow` siblings.
    grow: Bounds,
    /// Fraction (0..1) of the parent size minus padding and child gaps.
    percent: f32,
    /// Exact size.
    fixed: f32,
};

pub const SizingKind = std.meta.Tag(SizingAxis);

/// Sizing on both axes.
pub const Sizing = struct {
    width: SizingAxis = .{ .fit = .{} },
    height: SizingAxis = .{ .fit = .{} },
};

pub fn kind(axis: SizingAxis) SizingKind {
    return std.meta.activeTag(axis);
}

/// Gap between the element's bounding box and its children.
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

/// Direction children are laid out in.
pub const Direction = enum {
    left_to_right,
    top_to_bottom,
};

pub const AlignX = enum { left, center, right };
pub const AlignY = enum { top, center, bottom };

/// How children are aligned inside their parent, per axis.
pub const Alignment = struct {
    x: AlignX = .left,
    y: AlignY = .top,
};

/// Layout settings for a container.
pub const Layout = struct {
    sizing: Sizing = .{},
    padding: Padding = .{},
    /// Gap between adjacent children along the layout axis.
    child_gap: f32 = 0,
    direction: Direction = .left_to_right,
    child_alignment: Alignment = .{},
};

pub const WrapMode = enum {
    /// Break lines on spaces (and explicit newlines).
    words,
    /// Break lines only on explicit newlines.
    newlines,
    /// Never break lines.
    none,
};

/// Text settings for a text element.
pub const Text = struct {
    wrap: WrapMode = .words,
    /// Line height in pixels; `0` uses the measured text height.
    line_height: f32 = 0,
    /// Extra horizontal space after every word.
    letter_spacing: f32 = 0,
    /// Opaque value handed to the measure callback, e.g. a font handle index.
    measure_key: u32 = 0,
};

/// A point on a box, used to attach floating elements.
pub const AttachPoint = struct {
    x: AlignX = .left,
    y: AlignY = .top,
};

pub const AttachTo = enum {
    none,
    /// Attach to the element's hierarchical parent.
    parent,
    /// Attach to the layout root.
    root,
};

/// Settings for an element taken out of its parent's flow and positioned
/// relative to another box.
pub const Floating = struct {
    attach_to: AttachTo = .none,
    /// Point on the floating element that is placed at `parent`.
    element: AttachPoint = .{},
    /// Point on the attached element it is placed at.
    parent: AttachPoint = .{},
    /// Added after attaching.
    offset: Point = .{},
};

/// A single element declaration.
pub const Declaration = struct {
    layout: Layout = .{},
    floating: Floating = .{},
    /// Only read for text elements.
    text: Text = .{},
    /// Caller data echoed back through `Node.declaration.user`.
    user: ?*anyopaque = null,
};

/// Measures `content` at its natural (unwrapped) size.
pub const MeasureFn = *const fn (context: ?*anyopaque, content: []const u8, measure_key: u32) Size;
