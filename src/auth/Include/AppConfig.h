#pragma once

#include <cstdint>
#include <string>

namespace auth
{
struct AppConfig
{
  std::string bind_ip = "0.0.0.0";
  std::uint16_t port  = 3724;

  std::string realm_name    = "Arenacraft";
  std::string realm_address = "127.0.0.1:8085";
  std::uint8_t realm_type   = 1;
  std::uint8_t realm_flags  = 0;
  float        realm_pop    = 0.5f;
  std::uint8_t realm_tz     = 1;
  std::uint8_t realm_id     = 1;

  std::string db_host = "127.0.0.1";
  std::uint16_t db_port = 3306;
  std::string db_user = "acore";
  std::string db_pass = "acore";
  std::string db_name = "acore_auth";
};
} // namespace auth
