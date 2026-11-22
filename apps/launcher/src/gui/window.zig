//! Top-level window, its message pump and the clay-backed layout pass.
//!
//! One process-wide window class and one brush are registered lazily. The
//! `Window` is heap allocated because Win32 stores the instance pointer in
//! `GWLP_USERDATA` for the whole lifetime of the `HWND`; moving it afterwards
//! would dangle. `deinit` frees it.
//!
//! The layout engine is immediate-mode, but a Win32 window only gets a
//! notification (`WM_SIZE`) when its size changes. `beginLayout` therefore
//! records the declarations in `plan`, and every re-layout replays that plan
//! into the engine and applies the resulting rects to the controls in one
//! `DeferWindowPos` batch.

const std = @import("std");
const lib = @import("lib");
const c = lib.win32.c;
const clay = @import("clay");

const geometry = @import("geometry.zig");
const layout = @import("layout.zig");
const button = @import("button.zig");
const label = @import("label.zig");

const allocator = std.heap.smp_allocator;

const class_name = "ArenacraftGuiWindow";

/// Neutral background behind the widgets.
const background_color: c.COLORREF = 0x00f0f0f0;

/// `IDC_ARROW`, spelled out because MinGW expresses it through
/// `MAKEINTRESOURCE`, which `@cImport` cannot translate.
const idc_arrow = 32512;

/// Icon ID from `launcher.rc`, embedded in launcher.exe. `MAKEINTRESOURCE` is
/// not translated, so the small integer is cast to a pointer directly.
const icon_resource_id = 1;

const font_count = 2;

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

/// One step of the recorded layout tree, replayed on every layout pass.
const Plan = union(enum) {
    open: layout.Container,
    close,
    button: *button.Button,
    label: *label.Label,
};

const Placement = struct {
    node: clay.NodeId,
    target: union(enum) {
        button: *button.Button,
        label: *label.Label,
    },
};

pub const Window = struct {
    hwnd: c.HWND = null,
    buttons: std.ArrayListUnmanaged(*button.Button) = .empty,
    labels: std.ArrayListUnmanaged(*label.Label) = .empty,
    fonts: [font_count]c.HFONT = [_]c.HFONT{null} ** font_count,
    /// Screen DC used only to measure text; never drawn into.
    measure_dc: c.HDC = null,

    engine: clay.Layout,
    plan: std.ArrayListUnmanaged(Plan) = .empty,
    placements: std.ArrayListUnmanaged(Placement) = .empty,
    recording: bool = false,
    ready: bool = false,
    /// Open containers in the tree being recorded, to catch unbalanced trees.
    depth: usize = 0,

    /// Start recording a layout tree. Call `endLayout` after declaring it.
    pub fn beginLayout(self: *Window) !void {
        if (self.hwnd == null) return error.WindowDestroyed;
        if (self.recording) return error.LayoutAlreadyStarted;
        self.plan.clearRetainingCapacity();
        self.placements.clearRetainingCapacity();
        self.depth = 0;
        self.recording = true;
    }

    /// Open a container. Pair with `endContainer`.
    pub fn container(self: *Window, style: layout.Container) !void {
        if (!self.recording) return error.LayoutNotStarted;
        try self.plan.append(allocator, .{ .open = style });
        self.depth += 1;
    }

    /// Close the most recent container opened with `container`.
    pub fn endContainer(self: *Window) !void {
        if (!self.recording) return error.LayoutNotStarted;
        if (self.depth == 0) return error.UnbalancedLayout;
        try self.plan.append(allocator, .close);
        self.depth -= 1;
    }

    /// Add a push button as a child of the currently open container.
    pub fn addButton(self: *Window, options: button.Options) !*button.Button {
        if (self.hwnd == null) return error.WindowDestroyed;
        if (!self.recording) return error.LayoutNotStarted;

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

        if (self.fonts[@intFromEnum(label.Font.body)] != null) {
            _ = c.SendMessageA(child, c.WM_SETFONT, @intFromPtr(self.fonts[@intFromEnum(label.Font.body)]), c.TRUE);
        }

        const measured = measureText(self, options.text, @intFromEnum(label.Font.body));

        const widget = allocator.create(button.Button) catch |err| {
            _ = c.DestroyWindow(child);
            return err;
        };
        widget.* = .{
            .hwnd = child,
            .callback = options.onClick,
            .sizing = options.sizing,
            .padding = options.padding,
            .text_size = .{
                .width = layout.roundToInt(measured.width),
                .height = layout.roundToInt(measured.height),
            },
        };

        self.buttons.append(allocator, widget) catch |err| {
            allocator.destroy(widget);
            _ = c.DestroyWindow(child);
            return err;
        };
        self.plan.append(allocator, .{ .button = widget }) catch |err| {
            self.buttons.items.len -= 1;
            allocator.destroy(widget);
            _ = c.DestroyWindow(child);
            return err;
        };

        // Dialog-style tab handling is not wired up yet, so give the first
        // control focus explicitly: the button then responds to Space.
        if (self.buttons.items.len == 1) {
            _ = c.SetFocus(child);
        }
        return widget;
    }

    /// Add a static text label as a child of the currently open container.
    pub fn addLabel(self: *Window, options: label.Options) !*label.Label {
        if (self.hwnd == null) return error.WindowDestroyed;
        if (!self.recording) return error.LayoutNotStarted;

        const text = try allocator.dupeZ(u8, options.text);

        const style: c.DWORD = @intCast(c.WS_CHILD | c.WS_VISIBLE | c.SS_LEFT);
        const child = c.CreateWindowExA(
            0,
            "STATIC",
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
        if (child == null) {
            allocator.free(text);
            return error.ControlCreationFailed;
        }

        const font = self.fonts[@intFromEnum(options.font)];
        if (font != null) {
            _ = c.SendMessageA(child, c.WM_SETFONT, @intFromPtr(font), c.TRUE);
        }

        const widget = allocator.create(label.Label) catch |err| {
            allocator.free(text);
            _ = c.DestroyWindow(child);
            return err;
        };
        widget.* = .{
            .hwnd = child,
            .text = text,
            .font = options.font,
            .wrap = options.wrap,
            .sizing = options.sizing,
        };

        self.labels.append(allocator, widget) catch |err| {
            allocator.free(text);
            allocator.destroy(widget);
            _ = c.DestroyWindow(child);
            return err;
        };
        self.plan.append(allocator, .{ .label = widget }) catch |err| {
            self.labels.items.len -= 1;
            allocator.free(text);
            allocator.destroy(widget);
            _ = c.DestroyWindow(child);
            return err;
        };
        return widget;
    }

    /// Finish recording and apply the layout. The tree is replayed on resize.
    pub fn endLayout(self: *Window) !void {
        if (!self.recording) return error.LayoutNotStarted;
        const balanced = self.depth == 0;
        self.recording = false;
        self.depth = 0;
        if (!balanced) return error.UnbalancedLayout;
        self.ready = true;
        try self.relayout();
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
        for (self.labels.items) |widget| {
            allocator.free(widget.text);
            allocator.destroy(widget);
        }
        self.labels.deinit(allocator);
        self.plan.deinit(allocator);
        self.placements.deinit(allocator);
        self.engine.deinit();
        for (&self.fonts) |*font| {
            if (font.* != null) {
                _ = c.DeleteObject(@ptrCast(font.*));
                font.* = null;
            }
        }
        if (self.measure_dc != null) {
            _ = c.ReleaseDC(null, self.measure_dc);
            self.measure_dc = null;
        }
        allocator.destroy(self);
    }

    /// Re-run the recorded tree for the current client size.
    fn relayout(self: *Window) !void {
        const hwnd = self.hwnd orelse return;
        if (!self.ready) return;

        var client: c.RECT = undefined;
        if (c.GetClientRect(hwnd, &client) == 0) return;

        try self.runPlan(.{
            .width = @floatFromInt(@max(0, client.right - client.left)),
            .height = @floatFromInt(@max(0, client.bottom - client.top)),
        });
        self.applyPlacements();
    }

    fn runPlan(self: *Window, size: clay.Size) !void {
        self.placements.clearRetainingCapacity();
        try self.engine.begin(size);

        for (self.plan.items) |item| {
            switch (item) {
                .open => |style| {
                    _ = try self.engine.open(.{ .layout = layout.toClayContainer(style) });
                },
                .close => try self.engine.close(),
                .button => |widget| {
                    const node = try self.engine.open(buttonDeclaration(widget));
                    try self.engine.close();
                    try self.placements.append(allocator, .{ .node = node, .target = .{ .button = widget } });
                },
                .label => |widget| {
                    // The text sits inside a container so labels accept the same
                    // sizing options as any other widget; a fit container
                    // around a text element is exactly the text's size.
                    const node = try self.engine.open(.{ .layout = .{
                        .sizing = layout.toClaySizing(widget.sizing),
                    } });
                    _ = try self.engine.text(widget.text, .{
                        .wrap = layout.toClayWrap(widget.wrap),
                        .measure_key = @intFromEnum(widget.font),
                    });
                    try self.engine.close();
                    try self.placements.append(allocator, .{ .node = node, .target = .{ .label = widget } });
                },
            }
        }

        try self.engine.compute();
    }

    /// Move every control to its computed rect in one repaint.
    fn applyPlacements(self: *Window) void {
        if (self.placements.items.len == 0) return;

        var handle = c.BeginDeferWindowPos(@intCast(self.placements.items.len));
        if (handle == null) {
            // Batching is an optimisation; without it, move one by one.
            for (self.placements.items) |placement| {
                const rect = layout.toRect(self.engine.bounds(placement.node));
                self.storeRect(placement, rect);
                _ = c.MoveWindow(self.hwndOf(placement), rect.x, rect.y, rect.width, rect.height, c.TRUE);
            }
            return;
        }

        for (self.placements.items) |placement| {
            const rect = layout.toRect(self.engine.bounds(placement.node));
            self.storeRect(placement, rect);

            const next = c.DeferWindowPos(
                handle,
                self.hwndOf(placement),
                null,
                rect.x,
                rect.y,
                rect.width,
                rect.height,
                c.SWP_NOZORDER | c.SWP_NOACTIVATE,
            );
            if (next == null) {
                _ = c.EndDeferWindowPos(handle);
                return;
            }
            handle = next;
        }

        _ = c.EndDeferWindowPos(handle);
    }

    fn hwndOf(_: *Window, placement: Placement) c.HWND {
        return switch (placement.target) {
            .button => |widget| widget.hwnd,
            .label => |widget| widget.hwnd,
        };
    }

    fn storeRect(_: *Window, placement: Placement, rect: geometry.Rect) void {
        switch (placement.target) {
            .button => |widget| widget.setRect(rect),
            .label => |widget| widget.setRect(rect),
        }
    }
};

/// Give `.fit` buttons a minimum size derived from their measured label, so the
/// native control always has room for its text.
fn buttonDeclaration(widget: *button.Button) clay.Declaration {
    var declaration = clay.Declaration{ .layout = .{
        .sizing = layout.toClaySizing(widget.sizing),
        .padding = layout.toClayPadding(widget.padding),
    } };

    const minimum_width = @as(f32, @floatFromInt(widget.text_size.width)) +
        widget.padding.left + widget.padding.right;
    const minimum_height = @as(f32, @floatFromInt(widget.text_size.height)) +
        widget.padding.top + widget.padding.bottom;

    switch (declaration.layout.sizing.width) {
        .fit => |bounds| declaration.layout.sizing.width = .{ .fit = .{
            .min = @max(bounds.min, minimum_width),
            .max = bounds.max,
        } },
        else => {},
    }
    switch (declaration.layout.sizing.height) {
        .fit => |bounds| declaration.layout.sizing.height = .{ .fit = .{
            .min = @max(bounds.min, minimum_height),
            .max = bounds.max,
        } },
        else => {},
    }

    return declaration;
}

/// Layout's text measurement callback: measure with the widget's font.
fn measureText(context: ?*anyopaque, content: []const u8, measure_key: u32) clay.Size {
    const window: *Window = @ptrCast(@alignCast(context orelse return .{}));
    if (window.measure_dc == null) return .{};
    if (measure_key >= font_count) return .{};
    const font = window.fonts[measure_key];
    if (font == null) return .{};

    // UTF-8 is never shorter than the UTF-16 it encodes, so this bound is
    // enough for the stack path; a longer word takes the heap path instead of
    // overflowing the buffer.
    var buffer: [512]u16 = undefined;
    if (content.len <= buffer.len) {
        const length = std.unicode.utf8ToUtf16Le(&buffer, content) catch return .{};
        return textExtent(window, font, buffer[0..length]);
    }

    const wide = std.unicode.utf8ToUtf16LeAllocZ(allocator, content) catch return .{};
    defer allocator.free(wide);
    return textExtent(window, font, wide);
}

fn textExtent(window: *Window, font: c.HFONT, text: []const u16) clay.Size {
    _ = c.SelectObject(window.measure_dc, font);

    var size: c.SIZE = .{ .cx = 0, .cy = 0 };
    if (c.GetTextExtentPoint32W(window.measure_dc, text.ptr, @intCast(text.len), &size) == 0) return .{};

    return .{
        .width = @floatFromInt(size.cx),
        .height = @floatFromInt(size.cy),
    };
}

/// Create and show the window. The returned pointer is owned by the caller and
/// must be released with `Window.deinit`.
pub fn create(options: Options) !*Window {
    ensureDpiAware();
    try ensureClass();

    const self = try allocator.create(Window);
    self.* = .{ .engine = clay.Layout.init(allocator, measureText, self) };
    errdefer {
        self.engine.deinit();
        allocator.destroy(self);
    }

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

    self.fonts[@intFromEnum(label.Font.body)] = createFont(-18, c.FW_NORMAL);
    self.fonts[@intFromEnum(label.Font.title)] = createFont(-30, c.FW_BOLD);
    self.measure_dc = c.GetDC(null);

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

/// Load one icon size from the executable's resource table. The handles are
/// owned by the window class and live for the whole process.
fn loadIcon(metric: c_int) c.HICON {
    const size = c.GetSystemMetrics(metric);
    return @ptrCast(@alignCast(c.LoadImageA(
        c.GetModuleHandleA(null),
        @ptrFromInt(@as(usize, icon_resource_id)),
        c.IMAGE_ICON,
        size,
        size,
        c.LR_DEFAULTCOLOR,
    )));
}

fn ensureClass() !void {
    if (class_registered) return;

    // The brush lives as long as the class, i.e. for the whole process.
    class_brush = c.CreateSolidBrush(background_color);

    var window_class: c.WNDCLASSEXA = std.mem.zeroes(c.WNDCLASSEXA);
    window_class.cbSize = @sizeOf(c.WNDCLASSEXA);
    window_class.style = @intCast(c.CS_HREDRAW | c.CS_VREDRAW);
    window_class.lpfnWndProc = windowProc;
    window_class.hInstance = c.GetModuleHandleA(null);
    window_class.hCursor = c.LoadCursorA(null, @ptrFromInt(@as(usize, idc_arrow)));
    // Title bar, Alt+Tab and the taskbar take the icons from the class; both
    // handles live for the whole process, like the class brush.
    window_class.hIcon = loadIcon(c.SM_CXICON);
    window_class.hIconSm = loadIcon(c.SM_CXSMICON);
    window_class.hbrBackground = class_brush;
    window_class.lpszClassName = class_name;

    if (c.RegisterClassExA(&window_class) == 0) return error.WindowClassRegistrationFailed;
    class_registered = true;
}

fn ensureDpiAware() void {
    if (dpi_aware) return;
    // Best effort: older Windows versions simply ignore it.
    _ = c.SetProcessDPIAware();
    dpi_aware = true;
}

fn createFont(height: i32, weight: c_int) c.HFONT {
    return c.CreateFontW(
        height,
        0,
        0,
        0,
        weight,
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
            // Can fail only on allocation failure; the previous layout stays
            // applied, which is the least-bad outcome for a resize event.
            window.relayout() catch {};
            return 0;
        },
        c.WM_CTLCOLORSTATIC => {
            // Paint label backgrounds with the window brush so they blend in.
            if (class_brush) |brush| return @bitCast(@intFromPtr(brush));
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
