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
#include "argparse.h"

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
  argparse::ArgumentParser parser("arenacraft-auth");


  try
  {
    parser.add_description("Arenacraft auth server");

    parser.add_argument("--bind")
        .help("bind IPv4/IPv6 address")
        .default_value(cfg->bind_ip)
        .store_into(cfg->bind_ip);

    parser.add_argument("--port")
        .help("auth listening port")
        .default_value(cfg->port)
        .store_into(cfg->port);

    parser.add_argument("--realm-name")
        .help("realm display name")
        .default_value(cfg->realm_name)
        .store_into(cfg->realm_name);

    parser.add_argument("--realm-address")
        .help("realm host:port advertised to clients")
        .default_value(cfg->realm_address)
        .store_into(cfg->realm_address);

    parser.add_argument("--db-host")
        .help("MySQL host")
        .default_value(cfg->db_host)
        .store_into(cfg->db_host);

    parser.add_argument("--db-port")
        .help("MySQL port")
        .default_value(cfg->db_port)
        .store_into(cfg->db_port);

    parser.add_argument("--db-user")
        .help("MySQL user")
        .default_value(cfg->db_user)
        .store_into(cfg->db_user);

    parser.add_argument("--db-pass")
        .help("MySQL password")
        .default_value(cfg->db_pass)
        .store_into(cfg->db_pass);

    parser.add_argument("--db-name")
        .help("MySQL schema")
        .default_value(cfg->db_name)
        .store_into(cfg->db_name);

    parser.parse_args(argc, argv);

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
