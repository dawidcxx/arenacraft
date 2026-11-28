/*
 * C ABI between the `ac` router's C++ argument parsing and the Zig
 * launcher-server implementation (main.zig).
 *
 * Keep the struct layout in sync with `Config` in main.zig. `root`,
 * `data_dir` and `assets_dir` are NUL-terminated paths; the pointers follow
 * the port and are naturally aligned.
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
    char const*   data_dir;
    char const*   assets_dir;
  };

  int launcherServerRun(struct LauncherServerConfig const* config);

#ifdef __cplusplus
}
#endif
