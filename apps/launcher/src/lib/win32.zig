//! Win32 bindings, translated straight from the MinGW-w64 headers that ship
//! with Zig's bundled libc. No Windows SDK required: the launcher is always
//! built for `x86_64-windows-gnu` (see `build.zig`).
//!
//! This file is only the translation. Anything with launcher semantics lives
//! in `src/gui` (widgets, windows) or `src/launcher` (app behaviour).
//!
//! `@cImport` needs the C headers, so the module has to link libc
//! (`link_libc = true` in `build.zig`).

pub const c = @cImport({
    // Skip the kitchen sink: lean headers + no min/max macros, the two defines
    // every sane translation unit sets before including windows.h.
    @cDefine("WIN32_LEAN_AND_MEAN", "1");
    @cDefine("NOMINMAX", "1");
    // MinGW's fortified string inlines (wcscat/wcscpy) translate into local
    // declarations that Zig 0.16 rejects in ReleaseSafe ("unused local
    // constant"). They are unused here and Zig has its own safety checks, so
    // disable fortify to keep the C import valid in every optimize mode.
    @cDefine("_FORTIFY_SOURCE", "0");
    @cInclude("windows.h");
});
