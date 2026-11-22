//! Clay, ported to Zig: the layout solver only.
//!
//! This is a port of [Clay](https://github.com/nicbarker/clay) v0.14's layout
//! engine, not a binding. Only the parts that compute boxes are here: element
//! declarations, sizing (fit/grow/percent/fixed), padding, child gaps,
//! alignment, text wrapping and floating elements. Rendering concepts (colors,
//! borders, clipping, render commands, transitions, pointer state) are absent.
//!
//! The port is deliberately small and dependency-free: it does not use libc,
//! the Win32 API or any global state, so it can be tested natively and reused
//! outside the launcher.
//!
//! Usage is one frame at a time, mirroring Clay's immediate-mode model:
//!
//! ```zig
//! var layout = clay.Layout.init(allocator, measureText, context);
//! defer layout.deinit();
//!
//! try layout.begin(.{ .width = 640, .height = 400 });
//! _ = try layout.open(.{ .layout = .{ .padding = .all(24) } });
//! _ = try layout.text("Hello", .{});
//! try layout.close();
//! try layout.compute();
//!
//! for (layout.nodes()) |node| {
//!     // node.bounds contains absolute pixel bounds; node.declaration.user is
//!     // whatever the declaration carried.
//! }
//! ```
//!
//! Read `layout.zig` for the algorithm and the documented deviations from Clay.

const std = @import("std");

pub const types = @import("types.zig");
pub const text = @import("text.zig");
pub const layout = @import("layout.zig");

pub const Layout = layout.Layout;
pub const Node = layout.Node;
pub const NodeId = layout.NodeId;

pub const Point = types.Point;
pub const Size = types.Size;
pub const Rect = types.Rect;
pub const Bounds = types.Bounds;
pub const SizingAxis = types.SizingAxis;
pub const Sizing = types.Sizing;
pub const Padding = types.Padding;
pub const Direction = types.Direction;
pub const AlignX = types.AlignX;
pub const AlignY = types.AlignY;
pub const Alignment = types.Alignment;
pub const WrapMode = types.WrapMode;
pub const Text = types.Text;
pub const AttachPoint = types.AttachPoint;
pub const AttachTo = types.AttachTo;
pub const Floating = types.Floating;
pub const Declaration = types.Declaration;
pub const MeasureFn = types.MeasureFn;

test {
    _ = @import("layout_test.zig");
}
