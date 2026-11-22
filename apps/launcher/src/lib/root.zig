//! Shared launcher library: Win32 bindings today, launcher logic (patching,
//! updates, API client) as it grows.
//!
//! Built for a Windows target only, see `build.zig`.

pub const win32 = @import("win32.zig");
