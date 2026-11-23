//! Single-line text input widget.
//!
//! Backed by a native `EDIT` control. The control's window procedure is
//! subclassed for one reason: a stock single-line edit ignores Enter, while the
//! launcher wants a typed realm address to be committed without reaching for
//! the mouse. Everything else is forwarded to the original procedure, so
//! selection, clipboard, IME and scrolling behave exactly like a stock edit.
//!
//! `Edit` is opaque for the same reason `Button` is: call sites never see the
//! `HWND`.

const lib = @import("lib");
const c = lib.win32.c;

const geometry = @import("geometry.zig");
const layout = @import("layout.zig");
const Callback = @import("callback.zig").Callback;

pub const Options = struct {
    text: []const u8 = "",
    onEnter: Callback,
    sizing: layout.Sizing = .{},
};

pub const Edit = struct {
    hwnd: c.HWND,
    callback: Callback,
    sizing: layout.Sizing,
    rect: geometry.Rect = .{},
    /// The control's original window procedure; everything but Enter is
    /// forwarded to it.
    previous_proc: c.WNDPROC = null,

    /// Record the bounds computed by the layout pass.
    pub fn setRect(self: *Edit, rect: geometry.Rect) void {
        self.rect = rect;
    }
};

/// Install the Enter hook on a freshly created control.
pub fn install(target: *Edit) void {
    const previous = c.SetWindowLongPtrW(
        target.hwnd,
        c.GWLP_WNDPROC,
        @intCast(@intFromPtr(&editProc)),
    );
    target.previous_proc = @ptrFromInt(@as(usize, @intCast(previous)));
    _ = c.SetWindowLongPtrW(target.hwnd, c.GWLP_USERDATA, @intCast(@intFromPtr(target)));
}

fn editProc(hwnd: c.HWND, message: c.UINT, wparam: c.WPARAM, lparam: c.LPARAM) callconv(.c) c.LRESULT {
    const raw = c.GetWindowLongPtrW(hwnd, c.GWLP_USERDATA);
    if (raw == 0) return c.DefWindowProcW(hwnd, message, wparam, lparam);

    const self: *Edit = @ptrFromInt(@as(usize, @intCast(raw)));
    if (message == c.WM_KEYDOWN and wparam == c.VK_RETURN) {
        self.callback.call();
        return 0;
    }
    return c.CallWindowProcW(self.previous_proc, hwnd, message, wparam, lparam);
}
