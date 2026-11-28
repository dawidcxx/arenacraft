/*
 * `ac launcher-server` command line front end.
 *
 * Parsing stays on the C++ side (like the other `ac` sub programs) and the
 * result is handed to the Zig implementation through the C ABI in
 * LauncherServer.h / main.zig.
 */

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

  LauncherServerConfig config{};
  config.port = parser.get<std::uint16_t>("--port");
  if (config.port == 0)
  {
    std::cerr << "invalid --port: must be in 1..65535" << std::endl;
    return 1;
  }

  std::string const root = parser.get<std::string>("--root");
  config.root            = root.empty() ? nullptr : root.c_str();

  return launcherServerRun(&config);
}
