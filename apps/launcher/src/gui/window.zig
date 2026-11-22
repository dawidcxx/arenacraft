//! Top-level window, its message pump and the placeholder layout pass.
//!
//! One process-wide window class and one brush are registered lazily. The
//! `Window` is heap allocated because Win32 stores the instance pointer in
//! `GWLP_USERDATA` for the whole lifetime of the `HWND`; moving it afterwards
//! would dangle. `deinit` frees it.

const std = @import("std");
const lib = @import("lib");
const c = lib.win32.c;

const geometry = @import("geometry.zig");
const button = @import("button.zig");

const allocator = std.heap.smp_allocator;

const class_name = "ArenacraftGuiWindow";

/// Neutral background behind the widgets.
const background_color: c.COLORREF = 0x00f0f0f0;

/// `IDC_ARROW`, spelled out because MinGW expresses it through
/// `MAKEINTRESOURCE`, which `@cImport` cannot translate.
const idc_arrow = 32512;

var class_registered = false;
var class_brush: c.HBRUSH = null;
var dpi_aware = false;
/// Windows created by this module and not yet destroyed. `run` ends when the
/// last one is gone.
var live_windows: usize = 0;

pub const Options = struct {
    title: []const u8 = "Arenacraft",
    /// Client-area size in pixels (at the system's primary DPI).
    size: geometry.Size = .{ .width = 640, .height = 400 },
};

pub const Window = struct {
    hwnd: c.HWND = null,
    buttons: std.ArrayListUnmanaged(*button.Button) = .empty,
    font: c.HFONT = null,

    /// Add a push button as a child of this window.
    pub fn addButton(self: *Window, options: button.Options) !*button.Button {
        if (self.hwnd == null) return error.WindowDestroyed;

        const text = try allocator.dupeZ(u8, options.text);
        defer allocator.free(text);

        // Child controls take their id from the HMENU parameter; it is null
        // here because notifications are routed by the child HWND in lParam.
        const style: c.DWORD = @intCast(c.WS_CHILD | c.WS_VISIBLE | c.WS_TABSTOP | c.BS_PUSHBUTTON);
        const child = c.CreateWindowExA(
            0,
            "BUTTON",
            text.ptr,
            style,
            0,
            0,
            0,
            0,
            self.hwnd,
            null,
            c.GetModuleHandleA(null),
            null,
        );
        if (child == null) return error.ControlCreationFailed;
        errdefer _ = c.DestroyWindow(child);

        if (self.font != null) {
            _ = c.SendMessageA(child, c.WM_SETFONT, @intFromPtr(self.font), c.TRUE);
        }

        const widget = try allocator.create(button.Button);
        errdefer allocator.destroy(widget);
        widget.* = .{ .hwnd = child, .callback = options.onClick };
        try self.buttons.append(allocator, widget);

        self.relayout();
        // Dialog-style tab handling is not wired up yet, so give the first
        // control focus explicitly: the button then responds to Space.
        if (self.buttons.items.len == 1) {
            _ = c.SetFocus(child);
        }
        return widget;
    }

    /// Destroy the native window. Safe to call more than once.
    pub fn close(self: *Window) void {
        if (self.hwnd != null) {
            _ = c.DestroyWindow(self.hwnd);
        }
    }

    /// Destroy the window, its widgets and this allocation.
    pub fn deinit(self: *Window) void {
        self.close();
        for (self.buttons.items) |widget| allocator.destroy(widget);
        self.buttons.deinit(allocator);
        if (self.font != null) {
            _ = c.DeleteObject(@ptrCast(self.font));
        }
        allocator.destroy(self);
    }

    /// Placeholder for the layout engine. Widgets currently form a single
    /// column and share the client height equally; once yoga is wired in, this
    /// becomes "run layout, hand each widget its computed rect".
    fn relayout(self: *Window) void {
        if (self.hwnd == null) return;
        var client: c.RECT = undefined;
        if (c.GetClientRect(self.hwnd, &client) == 0) return;

        const count: i32 = @intCast(self.buttons.items.len);
        if (count == 0) return;

        const margin: i32 = 24;
        const gap: i32 = 12;
        const width = @max(1, client.right - client.left - 2 * margin);
        const share = @max(1, @divTrunc(client.bottom - client.top - 2 * margin - gap * (count - 1), count));

        var y = margin;
        for (self.buttons.items) |widget| {
            widget.setRect(.{ .x = margin, .y = y, .width = width, .height = share });
            y += share + gap;
        }
    }
};

/// Create and show the window. The returned pointer is owned by the caller and
/// must be released with `Window.deinit`.
pub fn create(options: Options) !*Window {
    try ensureClass();
    ensureDpiAware();

    const self = try allocator.create(Window);
    errdefer allocator.destroy(self);
    self.* = .{};

    // Translate the wanted client size into an outer window rect, then center
    // it on the primary monitor.
    var rect = c.RECT{
        .left = 0,
        .top = 0,
        .right = options.size.width,
        .bottom = options.size.height,
    };
    const style: c.DWORD = @intCast(c.WS_OVERLAPPEDWINDOW | c.WS_CLIPCHILDREN);
    _ = c.AdjustWindowRect(&rect, style, c.FALSE);
    const outer_width = rect.right - rect.left;
    const outer_height = rect.bottom - rect.top;
    const x = @divTrunc(c.GetSystemMetrics(c.SM_CXSCREEN) - outer_width, 2);
    const y = @divTrunc(c.GetSystemMetrics(c.SM_CYSCREEN) - outer_height, 2);

    const title = try allocator.dupeZ(u8, options.title);
    defer allocator.free(title);

    const hwnd = c.CreateWindowExA(
        0,
        class_name,
        title.ptr,
        style,
        x,
        y,
        outer_width,
        outer_height,
        null,
        null,
        c.GetModuleHandleA(null),
        self,
    );
    if (hwnd == null) return error.WindowCreationFailed;

    self.font = createFont();

    _ = c.ShowWindow(hwnd, c.SW_SHOW);
    _ = c.UpdateWindow(hwnd);
    return self;
}

/// Run the message loop until the last window is destroyed.
pub fn run() void {
    var message: c.MSG = undefined;
    while (c.GetMessageA(&message, null, 0, 0) > 0) {
        _ = c.TranslateMessage(&message);
        _ = c.DispatchMessageA(&message);
    }
}

fn ensureClass() !void {
    if (class_registered) return;

    // The brush lives as long as the class, i.e. for the whole process.
    class_brush = c.CreateSolidBrush(background_color);

    var window_class: c.WNDCLASSA = std.mem.zeroes(c.WNDCLASSA);
    window_class.style = @intCast(c.CS_HREDRAW | c.CS_VREDRAW);
    window_class.lpfnWndProc = windowProc;
    window_class.hInstance = c.GetModuleHandleA(null);
    window_class.hCursor = c.LoadCursorA(null, @ptrFromInt(@as(usize, idc_arrow)));
    window_class.hbrBackground = class_brush;
    window_class.lpszClassName = class_name;

    if (c.RegisterClassA(&window_class) == 0) return error.WindowClassRegistrationFailed;
    class_registered = true;
}

fn ensureDpiAware() void {
    if (dpi_aware) return;
    // Best effort: older Windows versions simply ignore it.
    _ = c.SetProcessDPIAware();
    dpi_aware = true;
}

fn createFont() c.HFONT {
    return c.CreateFontW(
        -20,
        0,
        0,
        0,
        c.FW_NORMAL,
        c.FALSE,
        c.FALSE,
        c.FALSE,
        c.DEFAULT_CHARSET,
        c.OUT_DEFAULT_PRECIS,
        c.CLIP_DEFAULT_PRECIS,
        c.CLEARTYPE_QUALITY,
        c.DEFAULT_PITCH,
        std.unicode.utf8ToUtf16LeStringLiteral("Segoe UI"),
    );
}

fn setUserData(hwnd: c.HWND, window: ?*Window) void {
    const address: usize = if (window) |value| @intFromPtr(value) else 0;
    _ = c.SetWindowLongPtrA(hwnd, c.GWLP_USERDATA, @intCast(address));
}

fn getUserData(hwnd: c.HWND) ?*Window {
    const value = c.GetWindowLongPtrA(hwnd, c.GWLP_USERDATA);
    if (value == 0) return null;
    return @ptrFromInt(@as(usize, @intCast(value)));
}

fn windowProc(hwnd: c.HWND, message: c.UINT, wparam: c.WPARAM, lparam: c.LPARAM) callconv(.c) c.LRESULT {
    if (message == c.WM_NCCREATE) {
        const create_struct: *const c.CREATESTRUCTA = @ptrFromInt(@as(usize, @bitCast(lparam)));
        if (create_struct.lpCreateParams) |params| {
            const self: *Window = @ptrCast(@alignCast(params));
            self.hwnd = hwnd;
            setUserData(hwnd, self);
            live_windows += 1;
        }
        return c.TRUE;
    }

    const window = getUserData(hwnd) orelse
        return c.DefWindowProcA(hwnd, message, wparam, lparam);

    switch (message) {
        c.WM_SIZE => {
            window.relayout();
            return 0;
        },
        c.WM_COMMAND => {
            const notification: c_int = @intCast((wparam >> 16) & 0xffff);
            if (notification == c.BN_CLICKED) {
                // lParam carries the child HWND that raised the notification.
                // Compare integer values: fabricating an `HWND` from it would
                // trip the pointer alignment check in Debug builds.
                const child: usize = @bitCast(lparam);
                for (window.buttons.items) |widget| {
                    if (@intFromPtr(widget.hwnd) == child) {
                        widget.callback.function(widget.callback.context);
                        break;
                    }
                }
            }
            return 0;
        },
        c.WM_NCDESTROY => {
            setUserData(hwnd, null);
            window.hwnd = null;
            // Counterpart of the WM_NCCREATE increment. Quitting on the last
            // window (instead of on any WM_DESTROY) keeps additional top-level
            // windows alive if the launcher ever grows one.
            live_windows -= 1;
            if (live_windows == 0) c.PostQuitMessage(0);
        },
        else => {},
    }

    return c.DefWindowProcA(hwnd, message, wparam, lparam);
}
