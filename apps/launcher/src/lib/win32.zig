//! Win32 bindings, translated straight from the MinGW-w64 headers that ship
//! with Zig's bundled libc. No Windows SDK required: the launcher is always
//! built for `x86_64-windows-gnu` (see `build.zig`).
//!
//! `@cImport` needs the C headers, so the module has to link libc
//! (`link_libc = true` in `build.zig`).

pub const c = @cImport({
    // Skip the kitchen sink: lean headers + no min/max macros, the two defines
    // every sane translation unit sets before including windows.h.
    @cDefine("WIN32_LEAN_AND_MEAN", "1");
    @cDefine("NOMINMAX", "1");
    @cInclude("windows.h");
});

/// Display a blocking message box (ANSI entry point, so Zig string literals
/// can be passed directly).
pub fn messageBox(title: [:0]const u8, text: [:0]const u8) void {
    _ = c.MessageBoxA(null, text.ptr, title.ptr, c.MB_OK | c.MB_ICONINFORMATION);
}
