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
//!
//! Declaration calls are infallible: `container`, the `add*` methods and
//! `endLayout` assert their invariants in debug builds and panic on a failed
//! native control, so view code never carries layout or control-creation
//! errors. `create` (window creation) and `spawn` (starting a task) still
//! return errors.
//!
//! Every widget kind goes through the same three steps (see `addButton`):
//! `createChild` makes the native control, the widget struct is allocated, and
//! `track` registers it for dispatch and for the layout plan. Kind-specific
//! behaviour lives in the `Control` switches below, not in per-kind lists.
//!
//! Background work is structured: `spawn` puts a task in the window's
//! `Io.Group`, and destroying the window cancels the group and waits for it.
//! Tasks reach the GUI thread through `Dispatcher.post` only (never a
//! synchronous `SendMessage`, which would deadlock against that wait).

const std = @import("std");
const lib = @import("lib");
const c = lib.win32.c;
const clay = @import("clay");

const geometry = @import("geometry.zig");
const layout = @import("layout.zig");
const button = @import("button.zig");
const edit = @import("edit.zig");
const label = @import("label.zig");
const listbox = @import("listbox.zig");
const Callback = @import("callback.zig").Callback;
const text_util = @import("text.zig");

const allocator = std.heap.smp_allocator;

const class_name = "ArenacraftGuiWindow";

/// Neutral background behind the widgets.
const background_color: c.COLORREF = 0x00f0f0f0;

/// Top-level window style: a normal resizable window that clips its children.
const window_style: c.DWORD = @intCast(c.WS_OVERLAPPEDWINDOW | c.WS_CLIPCHILDREN);

/// Face used by every control. Tahoma ships with every supported Windows and
/// is bundled by Wine; Segoe UI is not, and a substituted face can render with
/// different metrics than the measuring code assumed. Only the normal and bold
/// weights are requested so GDI never has to synthesise an in-between weight.
const font_face = std.unicode.utf8ToUtf16LeStringLiteral("Tahoma");

/// `IDC_ARROW`, spelled out because MinGW expresses it through
/// `MAKEINTRESOURCE`, which `@cImport` cannot translate.
const idc_arrow = 32512;

/// Icon ID from `launcher.rc`, embedded in launcher.exe. `MAKEINTRESOURCE` is
/// not translated, so the small integer is cast to a pointer directly.
const icon_resource_id = 1;

/// Private message that carries a heap-allocated `Callback` to the GUI thread.
const wm_post: c.UINT = c.WM_APP;

const font_count = @typeInfo(label.Font).@"enum".field_names.len;

var class_registered = false;
var class_brush: c.HBRUSH = null;
var dpi_aware = false;
/// Windows created by this module and not yet destroyed. `run` ends when the
/// last one is gone.
var live_windows: usize = 0;

pub const Options = struct {
    /// Runs the window's background tasks; must outlive the window.
    io: std.Io,
    title: []const u8 = "Arenacraft",
    /// Initial client-area size in pixels (at the system's primary DPI). The
    /// launcher passes its own compact, phone-shaped size; this is only a
    /// fallback.
    size: geometry.Size = .{ .width = 360, .height = 520 },
    /// Optional minimum client-area size. The player cannot drag the window
    /// below it; the compact column needs a floor to stay readable.
    min_size: ?geometry.Size = null,
};

/// A widget placed in the layout tree. The union is the single place that
/// knows how each kind exposes its handle, bounds and cleanup.
const Control = union(enum) {
    button: *button.Button,
    edit: *edit.Edit,
    label: *label.Label,
    list_box: *listbox.ListBox,

    fn hwnd(self: Control) c.HWND {
        return switch (self) {
            inline else => |widget| widget.hwnd,
        };
    }

    fn setRect(self: Control, rect: geometry.Rect) void {
        switch (self) {
            inline else => |widget| widget.setRect(rect),
        }
    }

    /// Free the widget struct and whatever it owns. The native control is the
    /// caller's business.
    fn free(self: Control) void {
        switch (self) {
            .label => |widget| {
                allocator.free(widget.text);
                allocator.destroy(widget);
            },
            inline else => |widget| allocator.destroy(widget),
        }
    }
};

/// One step of the recorded layout tree, replayed on every layout pass.
const Plan = union(enum) {
    open: layout.Container,
    close,
    control: Control,
};

const Placement = struct {
    node: clay.NodeId,
    control: Control,
};

/// Handle for waking the GUI thread from a task. It is a plain value (the
/// native handle), so it can be copied into a task. Posting to a destroyed
/// window fails instead of crashing.
pub const Dispatcher = struct {
    hwnd: c.HWND,

    /// Queue `callback` to run on the GUI thread, from any thread. The
    /// callback's context must stay valid until it runs; a callback still
    /// queued when the window is destroyed is dropped.
    pub fn post(self: Dispatcher, callback: Callback) !void {
        const job = allocator.create(Callback) catch unreachable;
        job.* = callback;
        if (c.PostMessageW(self.hwnd, wm_post, 0, @bitCast(@intFromPtr(job))) == 0) {
            allocator.destroy(job);
            return error.WindowDestroyed;
        }
    }
};

pub const Window = struct {
    hwnd: c.HWND = null,
    io: std.Io,
    /// Minimum client size enforced through `WM_GETMINMAXINFO`, if any.
    min_size: ?geometry.Size = null,
    /// Background tasks; cancelled and awaited when the window is destroyed.
    tasks: std.Io.Group = .init,
    controls: std.ArrayListUnmanaged(Control) = .empty,
    fonts: [font_count]c.HFONT = @splat(null),
    /// Screen DC used only to measure text; never drawn into.
    measure_dc: c.HDC = null,

    engine: clay.Layout,
    plan: std.ArrayListUnmanaged(Plan) = .empty,
    placements: std.ArrayListUnmanaged(Placement) = .empty,
    recording: bool = false,
    ready: bool = false,
    /// Open containers in the tree being recorded, to catch unbalanced trees.
    depth: usize = 0,
    /// Whether a control already took the initial keyboard focus. The first
    /// focusable control created inherits it, like a dialog's first tab stop.
    focus_assigned: bool = false,

    // -- Layout recording ---------------------------------------------------

    /// Start recording a layout tree. Call `endLayout` after declaring it.
    /// Infallible so the two can bracket a tree with `defer`: a missing window
    /// or an already-recording tree is a programmer error, not a runtime
    /// failure.
    pub fn beginLayout(self: *Window) void {
        std.debug.assert(self.hwnd != null);
        std.debug.assert(!self.recording);
        self.plan.clearRetainingCapacity();
        self.placements.clearRetainingCapacity();
        self.depth = 0;
        self.recording = true;
    }

    /// Open a container. Pair with `endContainer`.
    pub fn container(self: *Window, style: layout.Container) void {
        self.assertRecording();
        self.plan.append(allocator, .{ .open = style }) catch unreachable;
        self.depth += 1;
    }

    /// Close the most recent container opened with `container`.
    pub fn endContainer(self: *Window) void {
        self.assertRecording();
        std.debug.assert(self.depth > 0);
        self.plan.append(allocator, .close) catch unreachable;
        self.depth -= 1;
    }

    /// Empty element that soaks up the leftover space of the open container
    /// along `axis` (`.left_to_right` for a row, `.top_to_bottom` for a column),
    /// pushing the siblings after it to the far edge.
    pub fn spacer(self: *Window, axis: layout.Direction) void {
        self.container(.{ .sizing = switch (axis) {
            .left_to_right => .grow_width,
            .top_to_bottom => .grow_height,
        } });
        self.endContainer();
    }

    /// Finish recording and apply the layout. The tree is replayed on resize.
    /// Infallible like the declarations: leaving the bracket without closing
    /// every container is a bug, so debug builds stop here instead of applying
    /// a half-built tree.
    pub fn endLayout(self: *Window) void {
        std.debug.assert(self.recording);
        std.debug.assert(self.depth == 0);
        self.recording = false;
        self.ready = true;
        // A percent axis outside 0..1 is a declaration bug, not a recoverable
        // runtime failure.
        self.relayout() catch unreachable;
    }

    // -- Widgets ------------------------------------------------------------

    /// Add a push button as a child of the currently open container.
    pub fn addButton(self: *Window, options: button.Options) *button.Button {
        self.assertRecording();

        const child = self.createChild(
            std.unicode.utf8ToUtf16LeStringLiteral("BUTTON"),
            options.text,
            c.WS_TABSTOP | c.BS_PUSHBUTTON,
            .body,
        );

        const measured = measureText(self, options.text, @backingInt(label.Font.body));

        const widget = allocator.create(button.Button) catch unreachable;
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
        self.track(.{ .button = widget });

        // A window with no text field or list still needs a first tab stop, so
        // the button takes the initial focus. When a field exists it already
        // owns the focus (see `addEdit`).
        if (!self.has(.edit) and !self.has(.list_box)) self.takeFocus(child);
        return widget;
    }

    /// Add a single-selection list as a child of the currently open container.
    /// `onSelect` fires when a row is picked; see `listbox.zig`.
    pub fn addListBox(self: *Window, options: listbox.Options) *listbox.ListBox {
        self.assertRecording();

        const child = self.createChild(
            std.unicode.utf8ToUtf16LeStringLiteral("LISTBOX"),
            "",
            c.WS_TABSTOP | c.WS_VSCROLL | c.WS_BORDER | c.LBS_NOTIFY | c.LBS_NOINTEGRALHEIGHT,
            .body,
        );

        const widget = allocator.create(listbox.ListBox) catch unreachable;
        widget.* = .{
            .hwnd = child,
            .callback = options.onSelect,
            .sizing = options.sizing,
        };

        addListItems(widget, options.items);
        if (options.selected) |index| selectListItem(widget, index);

        self.track(.{ .list_box = widget });
        return widget;
    }

    /// Add a single-line text field as a child of the currently open container.
    /// `onEnter` fires when Enter is pressed; see `edit.zig`.
    pub fn addEdit(self: *Window, options: edit.Options) *edit.Edit {
        self.assertRecording();

        const child = self.createChild(
            std.unicode.utf8ToUtf16LeStringLiteral("EDIT"),
            options.text,
            c.WS_TABSTOP | c.WS_BORDER | c.ES_LEFT | c.ES_AUTOHSCROLL,
            .body,
        );

        const widget = allocator.create(edit.Edit) catch unreachable;
        widget.* = .{
            .hwnd = child,
            .callback = options.onEnter,
            .sizing = options.sizing,
        };
        edit.install(widget);

        self.track(.{ .edit = widget });
        self.takeFocus(child);
        return widget;
    }

    /// Add a static text label as a child of the currently open container.
    pub fn addLabel(self: *Window, options: label.Options) *label.Label {
        self.assertRecording();

        // A native STATIC wraps at word boundaries with `SS_LEFT`. For a
        // single-line label that is a trap: if the measured width is a pixel
        // short, the last word moves to a second line that the one-line control
        // clips, so the label looks truncated ("Arenacraft Launcher" renders as
        // "Arenacraft"). `.wrap = .none` therefore has to disable wrapping in
        // the control too. `SS_NOPREFIX` keeps a literal `&` in the text.
        const static_style: c.DWORD = @intCast(c.SS_NOPREFIX | switch (options.wrap) {
            .none => c.SS_LEFTNOWORDWRAP,
            .words, .newlines => c.SS_LEFT,
        });

        const child = self.createChild(
            std.unicode.utf8ToUtf16LeStringLiteral("STATIC"),
            options.text,
            static_style,
            options.font,
        );

        const text = allocator.dupeSentinel(u8, options.text, 0) catch unreachable;
        const widget = allocator.create(label.Label) catch unreachable;
        widget.* = .{
            .hwnd = child,
            .text = text,
            .font = options.font,
            .wrap = options.wrap,
            .sizing = options.sizing,
        };
        self.track(.{ .label = widget });
        return widget;
    }

    // -- Widget state -------------------------------------------------------

    /// Replace a list's rows and highlight `selected`. Used after a typed realm
    /// joins the remembered list. The native control copies the strings, so the
    /// caller keeps ownership and may free them afterwards. `LB_SETCURSEL` does
    /// not notify the parent, so this never re-enters the selection callback.
    pub fn setListItems(
        self: *Window,
        target: *listbox.ListBox,
        items: []const []const u8,
        selected: usize,
    ) void {
        _ = self;
        _ = c.SendMessageW(target.hwnd, c.LB_RESETCONTENT, 0, 0);

        addListItems(target, items);
        if (items.len > 0) {
            selectListItem(target, @min(selected, items.len - 1));
        }
    }

    /// Index of the highlighted row, or null when nothing is selected.
    pub fn listSelection(self: *Window, target: *listbox.ListBox) ?usize {
        _ = self;
        const result = c.SendMessageW(target.hwnd, c.LB_GETCURSEL, 0, 0);
        if (result == c.LB_ERR or result < 0) return null;
        return @intCast(result);
    }

    /// Highlight `index` without touching the rows. Unlike `setListItems` this
    /// is safe from inside the list's own selection notification, where
    /// resetting the content would desynchronise the native selection.
    pub fn setListSelection(self: *Window, target: *listbox.ListBox, index: usize) void {
        _ = self;
        selectListItem(target, index);
    }

    /// Drop the list highlight, so no row looks selected while a new realm is
    /// being typed.
    pub fn clearListSelection(self: *Window, target: *listbox.ListBox) void {
        _ = self;
        _ = c.SendMessageW(target.hwnd, c.LB_SETCURSEL, @bitCast(@as(isize, -1)), 0);
    }

    /// Give an edit field the keyboard focus, e.g. after Add clears it.
    pub fn focusEdit(self: *Window, target: *edit.Edit) void {
        _ = self;
        _ = c.SetFocus(target.hwnd);
    }

    /// Replace an edit's text and select it, so a typed replacement starts on
    /// the next keystroke.
    pub fn setEditText(self: *Window, target: *edit.Edit, text: []const u8) void {
        _ = self;
        const wide = text_util.toWide(allocator, text);
        defer allocator.free(wide);

        _ = c.SendMessageW(target.hwnd, c.WM_SETTEXT, 0, @bitCast(@intFromPtr(wide.ptr)));
        _ = c.SendMessageW(target.hwnd, c.EM_SETSEL, 0, -1);
    }

    /// Current text of an edit field, allocated with the GUI allocator. The
    /// caller owns the result.
    pub fn editText(self: *Window, target: *edit.Edit) []u8 {
        _ = self;
        var wide: [1024]u16 = undefined;
        const length = c.GetWindowTextW(target.hwnd, &wide, wide.len);
        if (length <= 0) return allocator.alloc(u8, 0) catch unreachable;
        return std.unicode.wtf16LeToWtf8Alloc(allocator, wide[0..@intCast(length)]) catch unreachable;
    }

    /// Replace a label's text and re-run the layout, so a wider status line
    /// still fits the tree. The label keeps an owned copy of the string.
    pub fn setLabelText(self: *Window, target: *label.Label, text: []const u8) void {
        const copy = allocator.dupeSentinel(u8, text, 0) catch unreachable;
        const wide = text_util.toWide(allocator, text);
        defer allocator.free(wide);

        allocator.free(target.text);
        target.text = copy;
        _ = c.SetWindowTextW(target.hwnd, wide.ptr);

        // The tree was validated by `endLayout`, so replaying it cannot fail.
        self.relayout() catch unreachable;
    }

    // -- Lifetime -----------------------------------------------------------

    /// Run `function(args...)` on a worker thread. The window cancels it when
    /// it is destroyed: the task stops at its next `Io` call with
    /// `error.Canceled` (CPU-bound loops should call `io.checkCancel()`), so
    /// it must not hold the GUI thread up. Report results with `dispatcher`.
    /// `function` must return `void` or an error set coercible to
    /// `Io.Cancelable!void`.
    pub fn spawn(self: *Window, function: anytype, args: std.meta.ArgsTuple(@TypeOf(function))) !void {
        try self.tasks.concurrent(self.io, function, args);
    }

    /// Handle tasks use to run code on the GUI thread.
    pub fn dispatcher(self: *Window) Dispatcher {
        return .{ .hwnd = self.hwnd };
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
        self.clear();
        self.controls.deinit(allocator);
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

    /// Destroy every control and forget the recorded tree, keeping the window.
    /// The caller records a new tree with `beginLayout`/`endLayout` afterwards;
    /// the launcher uses this to switch views.
    pub fn clear(self: *Window) void {
        // When the native window is gone its children died with it; only the
        // bookkeeping is left to free.
        const destroy = self.hwnd != null;
        for (self.controls.items) |control| {
            if (destroy) _ = c.DestroyWindow(control.hwnd());
            control.free();
        }
        self.controls.clearRetainingCapacity();

        self.plan.clearRetainingCapacity();
        self.placements.clearRetainingCapacity();
        self.recording = false;
        self.ready = false;
        self.depth = 0;
        self.focus_assigned = false;
    }

    // -- Internals ----------------------------------------------------------

    /// Cancel and await every task, then drop callbacks they left queued.
    /// Runs while the handle is still valid, so no task can post afterwards.
    fn cancelTasks(self: *Window, hwnd: c.HWND) void {
        self.tasks.cancel(self.io);

        var message: c.MSG = undefined;
        while (c.PeekMessageW(&message, hwnd, wm_post, wm_post, c.PM_REMOVE) != 0) {
            const job: *Callback = @ptrFromInt(@as(usize, @bitCast(message.lParam)));
            allocator.destroy(job);
        }
    }

    /// Invariants every declaration has: the window is alive and a tree is
    /// being recorded. Violating them is a bug in the caller, not a runtime
    /// condition, so debug builds stop here instead of returning an error.
    fn assertRecording(self: *const Window) void {
        std.debug.assert(self.hwnd != null);
        std.debug.assert(self.recording);
    }

    /// Create a visible native child with the font of `font` applied.
    /// `extra_style` is OR-ed onto `WS_CHILD | WS_VISIBLE`.
    fn createChild(
        self: *Window,
        class: [*:0]const u16,
        text: []const u8,
        extra_style: anytype,
        font: label.Font,
    ) c.HWND {
        const wide = text_util.toWide(allocator, text);
        defer allocator.free(wide);

        // Child controls take their id from the HMENU parameter; it is null
        // here because notifications are routed by the child HWND in lParam.
        const style: c.DWORD = @intCast(c.WS_CHILD | c.WS_VISIBLE | @as(c_long, @intCast(extra_style)));
        const child = c.CreateWindowExW(
            0,
            class,
            wide.ptr,
            style,
            0,
            0,
            0,
            0,
            self.hwnd,
            null,
            c.GetModuleHandleA(null),
            null,
        ) orelse @panic("gui: could not create a native child control");

        const handle = self.fonts[@backingInt(font)];
        if (handle != null) {
            _ = c.SendMessageW(child, c.WM_SETFONT, @intFromPtr(handle), c.TRUE);
        }
        return child;
    }

    /// Register a widget for message dispatch and for the layout plan.
    fn track(self: *Window, control: Control) void {
        self.controls.append(allocator, control) catch unreachable;
        self.plan.append(allocator, .{ .control = control }) catch unreachable;
    }

    fn has(self: *const Window, comptime kind: std.meta.Tag(Control)) bool {
        for (self.controls.items) |control| {
            if (control == kind) return true;
        }
        return false;
    }

    /// Hand the initial keyboard focus to `child`, unless an earlier focusable
    /// control already took it.
    fn takeFocus(self: *Window, child: c.HWND) void {
        if (self.focus_assigned) return;
        _ = c.SetFocus(child);
        self.focus_assigned = true;
    }

    /// Re-run the recorded tree for the current client size.
    fn relayout(self: *Window) error{InvalidPercent}!void {
        const hwnd = self.hwnd orelse return;
        if (!self.ready) return;

        var client: c.RECT = undefined;
        if (c.GetClientRect(hwnd, &client) == 0) return;

        self.runPlan(.{
            .width = @floatFromInt(@max(0, client.right - client.left)),
            .height = @floatFromInt(@max(0, client.bottom - client.top)),
        }) catch |err| switch (err) {
            error.OutOfMemory => unreachable,
            error.InvalidPercent => return error.InvalidPercent,
        };
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
                .control => |control| {
                    const node = try self.declare(control);
                    self.placements.append(allocator, .{ .node = node, .control = control }) catch unreachable;
                },
            }
        }

        try self.engine.compute();
    }

    /// Declare one control as a leaf of the engine tree.
    fn declare(self: *Window, control: Control) !clay.NodeId {
        const node = switch (control) {
            .button => |widget| try self.engine.open(buttonDeclaration(widget)),
            .edit => |widget| try self.engine.open(.{ .layout = .{
                .sizing = layout.toClaySizing(widget.sizing),
            } }),
            .list_box => |widget| try self.engine.open(.{ .layout = .{
                .sizing = layout.toClaySizing(widget.sizing),
            } }),
            .label => |widget| node: {
                // A STATIC clips its text at the control's right edge, and
                // `GetTextExtentPoint32W` reports glyph advances without the
                // final glyph's overhang. Leave room for that, and a little
                // more for uppercase text: its glyphs sit tighter against the
                // advance edge, so a cap-height label loses the most pixels.
                // Bottom slack covers the descender/rounding edge.
                const right_slack: f32 = if (isUppercase(widget.text)) 8 else 4;
                const node = try self.engine.open(.{ .layout = .{
                    .sizing = layout.toClaySizing(widget.sizing),
                    .padding = .{ .right = right_slack, .bottom = 2 },
                } });
                _ = try self.engine.text(widget.text, .{
                    .wrap = layout.toClayWrap(widget.wrap),
                    .measure_key = @backingInt(widget.font),
                });
                break :node node;
            },
        };
        try self.engine.close();
        return node;
    }

    /// Move every control to its computed rect in one repaint.
    fn applyPlacements(self: *Window) void {
        if (self.placements.items.len == 0) return;

        var handle = c.BeginDeferWindowPos(@intCast(self.placements.items.len));
        if (handle == null) {
            // Batching is an optimisation; without it, move one by one.
            for (self.placements.items) |placement| {
                const rect = layout.toRect(self.engine.bounds(placement.node));
                placement.control.setRect(rect);
                _ = c.MoveWindow(placement.control.hwnd(), rect.x, rect.y, rect.width, rect.height, c.TRUE);
            }
            return;
        }

        for (self.placements.items) |placement| {
            const rect = layout.toRect(self.engine.bounds(placement.node));
            placement.control.setRect(rect);

            const next = c.DeferWindowPos(
                handle,
                placement.control.hwnd(),
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

        // A STATIC that grew only repaints the strip it gained, so its old
        // word-wrapped text can stay on screen until something else repaints
        // it (e.g. "Arenacraft" -> "New realm" shows just "New"). Labels are
        // cheap: repaint them in full after every layout pass.
        for (self.placements.items) |placement| switch (placement.control) {
            .label => |widget| _ = c.InvalidateRect(widget.hwnd, null, c.TRUE),
            else => {},
        };
    }

    /// Route a `WM_COMMAND` notification to the widget whose handle raised it.
    /// The callback may destroy that widget (a view switch does), so nothing
    /// touches the control list after it runs.
    fn dispatchCommand(self: *Window, notification: c_int, child: usize) void {
        for (self.controls.items) |control| {
            if (@intFromPtr(control.hwnd()) != child) continue;
            switch (control) {
                .button => |widget| if (notification == c.BN_CLICKED) widget.callback.call(),
                .list_box => |widget| if (notification == c.LBN_SELCHANGE) widget.callback.call(),
                .edit, .label => {},
            }
            return;
        }
    }
};

fn addListItems(target: *listbox.ListBox, items: []const []const u8) void {
    for (items) |item| addListItem(target.hwnd, item);
}

fn addListItem(hwnd: c.HWND, text: []const u8) void {
    const wide = text_util.toWide(allocator, text);
    defer allocator.free(wide);

    const result = c.SendMessageW(hwnd, c.LB_ADDSTRING, 0, @bitCast(@intFromPtr(wide.ptr)));
    // The native list ran out of memory.
    if (result == @as(c.LRESULT, c.LB_ERR) or result == @as(c.LRESULT, c.LB_ERRSPACE)) unreachable;
}

fn selectListItem(target: *listbox.ListBox, index: usize) void {
    _ = c.SendMessageW(target.hwnd, c.LB_SETCURSEL, @intCast(index), 0);
}

/// True when `text` is set in caps: it has at least one ASCII letter and no
/// lowercase letters. Such a label needs more right-side clip room; see
/// `declare`.
fn isUppercase(text: []const u8) bool {
    var has_upper = false;
    for (text) |byte| {
        if (std.ascii.isLower(byte)) return false;
        if (std.ascii.isUpper(byte)) has_upper = true;
    }
    return has_upper;
}

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
    self.* = .{
        .io = options.io,
        .min_size = options.min_size,
        .engine = clay.Layout.init(allocator, measureText, self),
    };
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
    const style: c.DWORD = window_style;
    _ = c.AdjustWindowRect(&rect, style, c.FALSE);
    const outer_width = rect.right - rect.left;
    const outer_height = rect.bottom - rect.top;
    const x = @divTrunc(c.GetSystemMetrics(c.SM_CXSCREEN) - outer_width, 2);
    const y = @divTrunc(c.GetSystemMetrics(c.SM_CYSCREEN) - outer_height, 2);

    const title = try allocator.dupeSentinel(u8, options.title, 0);
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
    self.fonts[@backingInt(label.Font.body)] = createFont(-18, c.FW_NORMAL);
    self.fonts[@backingInt(label.Font.small)] = createFont(-16, c.FW_NORMAL);
    self.fonts[@backingInt(label.Font.small_bold)] = createFont(-16, c.FW_BOLD);
    // Sized so "Arenacraft Launcher" stays on one line in the minimum window.
    self.fonts[@backingInt(label.Font.title)] = createFont(-28, c.FW_BOLD);
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
    const handle = c.LoadImageA(
        c.GetModuleHandleA(null),
        @ptrFromInt(@as(usize, icon_resource_id)),
        c.IMAGE_ICON,
        size,
        size,
        c.LR_DEFAULTCOLOR,
    );
    return handleToIcon(handle);
}

/// `LoadImageA` returns an untyped `HANDLE`, while `HICON` points at a dummy
/// struct and so carries an alignment. Handle values are not required to be
/// aligned — Wine returns odd ones — so `@alignCast` trips the safety check.
/// Handles are never dereferenced: reinterpret the bits.
fn handleToIcon(handle: c.HANDLE) c.HICON {
    var icon: c.HICON = undefined;
    @memcpy(std.mem.asBytes(&icon), std.mem.asBytes(&handle));
    return icon;
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
        font_face,
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
        // Let the default proc run for WM_NCCREATE. Returning TRUE directly
        // skips it and leaves the caption passed to CreateWindowExA unapplied:
        // the window then has no title bar text.
        return c.DefWindowProcA(hwnd, message, wparam, lparam);
    }

    const window = getUserData(hwnd) orelse
        return c.DefWindowProcA(hwnd, message, wparam, lparam);

    switch (message) {
        c.WM_GETMINMAXINFO => {
            // Keep the compact column readable: translate the minimum client
            // size into the outer window size (caption and borders included).
            if (window.min_size) |minimum| {
                var rect = c.RECT{
                    .left = 0,
                    .top = 0,
                    .right = minimum.width,
                    .bottom = minimum.height,
                };
                _ = c.AdjustWindowRect(&rect, window_style, c.FALSE);

                const info: *c.MINMAXINFO = @ptrFromInt(@as(usize, @bitCast(lparam)));
                info.ptMinTrackSize.x = rect.right - rect.left;
                info.ptMinTrackSize.y = rect.bottom - rect.top;
                return 0;
            }
        },
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
            // lParam carries the child HWND that raised the notification.
            // Compare integer values: fabricating an `HWND` from it would
            // trip the pointer alignment check in Debug builds.
            window.dispatchCommand(notification, @bitCast(lparam));
            return 0;
        },
        wm_post => {
            const job: *Callback = @ptrFromInt(@as(usize, @bitCast(lparam)));
            const callback = job.*;
            allocator.destroy(job);
            callback.call();
            return 0;
        },
        c.WM_DESTROY => window.cancelTasks(hwnd),
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
