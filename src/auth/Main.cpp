#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <boost/asio.hpp>
#include <boost/cobalt/op.hpp>
#include <boost/cobalt/run.hpp>
#include <boost/cobalt/spawn.hpp>
#include <boost/cobalt/task.hpp>
#include <boost/cobalt/this_coro.hpp>
#include <boost/system/error_code.hpp>

#include "AppConfig.h"
#include "AuthSession.h"
#include "Logging.h"
#include "MySqlAsync.h"

namespace asio = boost::asio;
using tcp      = asio::ip::tcp;

namespace
{
boost::cobalt::task<void> run_server(std::shared_ptr<auth::AppConfig> cfg)
{
  auto executor = co_await boost::cobalt::this_coro::executor;

  auto& io_ctx = static_cast<asio::io_context&>(executor.context());

  auto db = std::make_shared<auth::MySqlAsync>(io_ctx, *cfg);
  if (!db->connect())
    throw std::runtime_error("failed to connect to auth MySQL database");

  tcp::acceptor acceptor(executor, tcp::endpoint(asio::ip::make_address(cfg->bind_ip), cfg->port));
  acceptor.set_option(tcp::acceptor::reuse_address(true));

  {
    std::ostringstream message;
    message << "Auth server listening on " << cfg->bind_ip << ':' << cfg->port << " | realm=" << cfg->realm_name
            << " @ " << cfg->realm_address;
    logging::write(logging::Level::info, message.str());
  }

  for (;;)
  {
    tcp::socket socket = co_await acceptor.async_accept(boost::cobalt::use_op);

    boost::system::error_code ec;
    const auto                remote = socket.remote_endpoint(ec);
    if (!ec)
    {
      std::ostringstream message;
      message << "Accepted auth connection from " << remote;
      logging::write(logging::Level::info, message.str());
    }

    auto session = std::make_shared<auth::AuthSession>(std::move(socket), db, cfg);
    boost::cobalt::spawn(
        executor,
        [session]() -> boost::cobalt::task<void>
        {
          co_await session->run();
        }(),
        [](std::exception_ptr ep)
        {
          if (!ep)
            return;
          try
          {
            std::rethrow_exception(ep);
          }
          catch (const std::exception& ex)
          {
            logging::write(logging::Level::error, std::string("Detached auth task error: ") + ex.what());
          }
        });
  }
}
} // namespace

int main(int argc, char** argv)
{
  auto cfg = std::make_shared<auth::AppConfig>();

  try
  {
    auto print_help = []()
    {
      std::cout << "arenacraft auth server options\n"
                   "  --bind <ip>\n"
                   "  --port <num>\n"
                   "  --realm-name <name>\n"
                   "  --realm-address <host:port>\n"
                   "  --db-host <host>\n"
                   "  --db-port <num>\n"
                   "  --db-user <user>\n"
                   "  --db-pass <pass>\n"
                   "  --db-name <schema>\n"
                   "  --help\n";
    };

    for (int i = 1; i < argc; ++i)
    {
      const std::string arg = argv[i];
      const auto require_value = [&](const std::string& name) -> std::string
      {
        if (i + 1 >= argc)
          throw std::runtime_error("missing value for argument: " + name);
        ++i;
        return argv[i];
      };

      if (arg == "--help" || arg == "-h")
      {
        print_help();
        return 0;
      }
      if (arg == "--bind")
        cfg->bind_ip = require_value(arg);
      else if (arg == "--port")
        cfg->port = static_cast<std::uint16_t>(std::stoul(require_value(arg)));
      else if (arg == "--realm-name")
        cfg->realm_name = require_value(arg);
      else if (arg == "--realm-address")
        cfg->realm_address = require_value(arg);
      else if (arg == "--db-host")
        cfg->db_host = require_value(arg);
      else if (arg == "--db-port")
        cfg->db_port = static_cast<std::uint16_t>(std::stoul(require_value(arg)));
      else if (arg == "--db-user")
        cfg->db_user = require_value(arg);
      else if (arg == "--db-pass")
        cfg->db_pass = require_value(arg);
      else if (arg == "--db-name")
        cfg->db_name = require_value(arg);
      else
        throw std::runtime_error("unknown argument: " + arg);
    }

    if (argc == 1)
    {
      logging::write(logging::Level::info,
                     "Starting with defaults. Use --help to list CLI options.");
    }

    if (cfg->port == 0 || cfg->db_port == 0)
      throw std::runtime_error("port values must be > 0");

    boost::cobalt::run(run_server(cfg));
  }
  catch (const std::exception& ex)
  {
    logging::write(logging::Level::error, std::string("Fatal server error: ") + ex.what());
    return 1;
  }

  return 0;
}
