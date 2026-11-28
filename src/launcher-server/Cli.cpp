/*
 * `ac launcher-server` command line front end.
 *
 * Parsing stays on the C++ side (like the other `ac` sub programs) and the
 * result is handed to the Zig implementation through the C ABI in
 * LauncherServer.h / main.zig.
 */

#include "Config.h"
#include "LauncherServer.h"

#include <argparse/argparse.hpp>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

namespace
{
constexpr std::uint16_t DefaultPort = 5815;

static_assert(offsetof(LauncherServerConfig, root) == sizeof(void*), "layout must match Config in main.zig");
static_assert(offsetof(LauncherServerConfig, data_dir) == sizeof(void*) * 2, "layout must match Config in main.zig");
static_assert(offsetof(LauncherServerConfig, assets_dir) == sizeof(void*) * 3, "layout must match Config in main.zig");
} // namespace

int launcher_server_main(int argc, char** argv)
{
  argparse::ArgumentParser parser("ac launcher-server", "1.0");
  parser.add_description("Serve the launcher over TCP (newline-delimited JSON envelopes plus optional raw bodies)");
  parser.add_argument("-p", "--port")
      .help("TCP port to listen on")
      .scan<'d', std::uint16_t>()
      .default_value(DefaultPort);
  parser.add_argument("--root")
      .help("directory exposed by the get_file command (file serving is disabled when unset)")
      .default_value(std::string(""));
  parser.add_argument("-e", "--env-file")
      .help("load environment variables from a KEY=VALUE file (default: ./.env if present)")
      .default_value(std::string(""));

  try
  {
    parser.parse_args(argc, argv);
  }
  catch (std::exception const& err)
  {
    std::cerr << err.what() << std::endl;
    std::cerr << parser << std::endl;
    return 1;
  }

  std::string const envFile = parser.get<std::string>("--env-file");
  if (!envFile.empty())
  {
    if (!sConfigMgr->ApplyEnvFile(envFile))
    {
      std::cerr << "Failed to open env file '" << envFile << "'" << std::endl;
      return 1;
    }
  }
  else
  {
    sConfigMgr->ApplyEnvFile(".env");
  }

  sConfigMgr->Configure();
  if (!sConfigMgr->LoadAppConfigs())
    return 1;

  LauncherServerConfig config{};
  config.port = parser.get<std::uint16_t>("--port");
  if (config.port == 0)
  {
    std::cerr << "invalid --port: must be in 1..65535" << std::endl;
    return 1;
  }

  std::string const root = parser.get<std::string>("--root");
  config.root            = root.empty() ? nullptr : root.c_str();

  // Patch inputs are configured by AC_DATA_DIR / AC_ASSETS_DIR only; keeping
  // them off the CLI avoids two code paths with different defaults.
  std::string const dataDir   = sConfigMgr->GetDataPath();
  std::string const assetsDir = sConfigMgr->GetAssetsPath();
  config.data_dir             = dataDir.c_str();
  config.assets_dir           = assetsDir.c_str();

  return launcherServerRun(&config);
}
