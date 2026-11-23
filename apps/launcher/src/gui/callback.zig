//! Type-erased callback shared by every widget event and `Window.post`.
//!
//! Native controls only carry a `?*anyopaque`, so the erasure has to happen
//! somewhere. `bind` does it once, in one place, and generates the cast thunk
//! at compile time: call sites pass a typed context and a typed function and
//! never write `@ptrCast(@alignCast(...))` themselves.
//!
//!     .onClick = .bind(app, onQuit),   // fn onQuit(app: *App) void

pub const Callback = struct {
    function: *const fn (context: ?*anyopaque) void,
    context: ?*anyopaque = null,

    /// Pair a pointer with a function taking that same pointer type.
    pub fn bind(context: anytype, comptime function: fn (@TypeOf(context)) void) Callback {
        const Pointer = @TypeOf(context);
        comptime if (@typeInfo(Pointer) != .pointer) @compileError("Callback.bind needs a pointer context");

        const Thunk = struct {
            fn call(raw: ?*anyopaque) void {
                const typed: Pointer = @ptrCast(@alignCast(raw.?));
                function(typed);
            }
        };
        return .{ .function = &Thunk.call, .context = context };
    }

    /// Like `bind`, for functions that take no context.
    pub fn plain(comptime function: fn () void) Callback {
        const Thunk = struct {
            fn call(_: ?*anyopaque) void {
                function();
            }
        };
        return .{ .function = &Thunk.call };
    }

    pub fn call(self: Callback) void {
        self.function(self.context);
    }
};
