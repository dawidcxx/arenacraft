#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/cobalt/task.hpp>

#include "AppConfig.h"
#include "AuthProtocol.h"
#include "Srp6Server.h"

namespace auth
{
namespace asio = boost::asio;
using tcp      = asio::ip::tcp;

class MySqlAsync;

class AuthSession
{
public:
  AuthSession(tcp::socket socket, std::shared_ptr<MySqlAsync> db, std::shared_ptr<AppConfig> cfg);

  boost::cobalt::task<void> run();

private:
  boost::cobalt::task<std::uint8_t> read_u8();
  boost::cobalt::task<std::vector<std::uint8_t>> read_exact(std::size_t n);
  boost::cobalt::task<void> write_packet(const std::vector<std::uint8_t>& packet);

  boost::cobalt::task<void> send_auth_error(std::uint8_t cmd, std::uint8_t code);

  boost::cobalt::task<void> handle_logon_challenge();
  boost::cobalt::task<void> handle_logon_proof();
  boost::cobalt::task<void> handle_realm_list();

  tcp::socket                 socket_;
  std::shared_ptr<MySqlAsync> db_;
  std::shared_ptr<AppConfig>  cfg_;
  bool                        authed_ = false;
  std::string                 login_;
  std::optional<AccountRow>   account_;
  std::optional<Srp6Server>   srp_;
};
} // namespace auth
