#include "MySqlAsync.h"

#include <cstdlib>
#include <cstring>
#include <sstream>

#include "Logging.h"

namespace auth
{
MySqlAsync::MySqlAsync(asio::io_context& io, AppConfig cfg)
    : io_(io), cfg_(std::move(cfg)), workers_(1)
{
}

MySqlAsync::~MySqlAsync()
{
  workers_.join();
  std::lock_guard<std::mutex> lock(mysql_mutex_);
  if (mysql_)
  {
    mysql_close(mysql_);
    mysql_ = nullptr;
  }
}

bool MySqlAsync::connect()
{
  std::lock_guard<std::mutex> lock(mysql_mutex_);

  mysql_ = mysql_init(nullptr);
  if (!mysql_)
    return false;

  unsigned int timeout_sec = 5;
  mysql_options(mysql_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout_sec);

  if (!mysql_real_connect(mysql_, cfg_.db_host.c_str(), cfg_.db_user.c_str(), cfg_.db_pass.c_str(), cfg_.db_name.c_str(),
                          cfg_.db_port, nullptr, 0))
  {
    logging::write(logging::Level::error, std::string("mysql_real_connect failed: ") + mysql_error(mysql_));
    mysql_close(mysql_);
    mysql_ = nullptr;
    return false;
  }

  return true;
}

std::optional<AccountRow> MySqlAsync::fetch_account_blocking(const std::string& login)
{
  std::lock_guard<std::mutex> lock(mysql_mutex_);
  if (!mysql_)
    return std::nullopt;

  std::string escaped(login.size() * 2 + 1, '\0');
  const auto escaped_len = mysql_real_escape_string(mysql_, escaped.data(), login.c_str(), static_cast<unsigned long>(login.size()));
  escaped.resize(escaped_len);

  std::ostringstream query;
  query << "SELECT id, username, v, s FROM account WHERE username = UPPER('" << escaped << "') LIMIT 1";

  if (mysql_query(mysql_, query.str().c_str()) != 0)
  {
    logging::write(logging::Level::error, std::string("mysql_query(fetch_account) failed: ") + mysql_error(mysql_));
    return std::nullopt;
  }

  MYSQL_RES* result = mysql_store_result(mysql_);
  if (!result)
  {
    logging::write(logging::Level::error, std::string("mysql_store_result(fetch_account) failed: ") + mysql_error(mysql_));
    return std::nullopt;
  }

  MYSQL_ROW row = mysql_fetch_row(result);
  if (!row)
  {
    mysql_free_result(result);
    return std::nullopt;
  }

  unsigned long* lengths = mysql_fetch_lengths(result);
  if (!lengths || !row[0] || !row[1] || !row[2] || !row[3] || lengths[2] != 32 || lengths[3] != 32)
  {
    mysql_free_result(result);
    return std::nullopt;
  }

  AccountRow out{};
  out.id = static_cast<std::uint32_t>(std::strtoul(row[0], nullptr, 10));
  out.username.assign(row[1], lengths[1]);
  std::memcpy(out.verifier.data(), row[2], 32);
  std::memcpy(out.salt.data(), row[3], 32);

  mysql_free_result(result);
  return out;
}

bool MySqlAsync::update_session_key_blocking(const std::uint32_t account_id, const std::array<std::uint8_t, 40>& session_key,
                                             const std::string& ip)
{
  std::lock_guard<std::mutex> lock(mysql_mutex_);
  if (!mysql_)
    return false;

  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string key_hex;
  key_hex.reserve(session_key.size() * 2);
  for (auto b : session_key)
  {
    key_hex.push_back(kHex[(b >> 4) & 0x0F]);
    key_hex.push_back(kHex[b & 0x0F]);
  }

  std::string escaped_ip(ip.size() * 2 + 1, '\0');
  const auto escaped_ip_len = mysql_real_escape_string(mysql_, escaped_ip.data(), ip.c_str(), static_cast<unsigned long>(ip.size()));
  escaped_ip.resize(escaped_ip_len);

  std::ostringstream query;
  query << "UPDATE account SET sessionkey='" << key_hex << "', last_ip='" << escaped_ip
        << "', last_login=NOW() WHERE id=" << account_id;

  if (mysql_query(mysql_, query.str().c_str()) != 0)
  {
    logging::write(logging::Level::error, std::string("mysql_query(update_session_key) failed: ") + mysql_error(mysql_));
    return false;
  }

  return true;
}
} // namespace auth
