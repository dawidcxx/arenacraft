//! Geometry vocabulary shared by the GUI layer and the layout engine.
//!
//! Widgets receive their bounds as a `Rect`; nothing in the public surface
//! knows how those rects are produced. `gui/layout.zig` converts the engine's
//! floating-point bounds to these whole pixels.

/// Offset inside a parent, in pixels.
pub const Point = struct {
    x: i32 = 0,
    y: i32 = 0,
};

/// Size in pixels.
pub const Size = struct {
    width: i32 = 0,
    height: i32 = 0,
};

/// Position and size in parent-relative pixels.
pub const Rect = struct {
    x: i32 = 0,
    y: i32 = 0,
    width: i32 = 0,
    height: i32 = 0,
};
