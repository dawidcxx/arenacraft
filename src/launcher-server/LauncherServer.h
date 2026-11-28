/*
 * C ABI between the `ac` router's C++ argument parsing and the Zig
 * launcher-server implementation (main.zig).
 *
 * Keep the struct layout in sync with `Config` in main.zig (`root` is a
 * NUL-terminated path or null; it follows a naturally aligned pointer).
 */

#pragma once

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C"
{
#endif

  struct LauncherServerConfig
  {
    std::uint16_t port;
    char const*   root;
  };

  int launcherServerRun(struct LauncherServerConfig const* config);

#ifdef __cplusplus
}
#endif
