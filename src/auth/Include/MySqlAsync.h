#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include <boost/asio.hpp>
#include <boost/asio/async_result.hpp>

#include "AppConfig.h"
#include "AuthProtocol.h"

#if __has_include(<mysql/mysql.h>)
#include <mysql/mysql.h>
#else
#include <mysql.h>
#endif

namespace auth
{
namespace asio = boost::asio;

class MySqlAsync
{
public:
  explicit MySqlAsync(asio::io_context& io, AppConfig cfg);
  ~MySqlAsync();

  bool connect();

  template <typename CompletionToken>
  auto async_fetch_account(std::string login, CompletionToken&& token)
  {
    return async_run([this, login = std::move(login)]() -> std::optional<AccountRow>
                     { return fetch_account_blocking(login); },
                     std::forward<CompletionToken>(token));
  }

  template <typename CompletionToken>
  auto async_update_session_key(std::uint32_t account_id, std::array<std::uint8_t, 40> session_key, std::string ip,
                                CompletionToken&& token)
  {
    return async_run([this, account_id, session_key, ip = std::move(ip)]() -> bool
                     { return update_session_key_blocking(account_id, session_key, ip); },
                     std::forward<CompletionToken>(token));
  }

private:
  std::optional<AccountRow> fetch_account_blocking(const std::string& login);
  bool update_session_key_blocking(std::uint32_t account_id, const std::array<std::uint8_t, 40>& session_key,
                                   const std::string& ip);

  template <typename WorkFn, typename CompletionToken>
  auto async_run(WorkFn&& work, CompletionToken&& token)
  {
    using result_t = std::invoke_result_t<WorkFn>;
    return asio::async_initiate<CompletionToken, void(result_t)>(
        [this, work = std::forward<WorkFn>(work)](auto handler) mutable
        {
          auto callback_executor = asio::get_associated_executor(handler, io_.get_executor());

          asio::post(workers_,
                     [work = std::move(work), handler = std::move(handler), callback_executor]() mutable
                     {
                       result_t result{};
                       try
                       {
                         result = work();
                       }
                       catch (...)
                       {
                       }

                       asio::post(callback_executor,
                                  [handler = std::move(handler), result = std::move(result)]() mutable
                                  {
                                    handler(std::move(result));
                                  });
                     });
        },
        std::forward<CompletionToken>(token));
  }

  asio::io_context& io_;
  AppConfig         cfg_;
  asio::thread_pool workers_;
  MYSQL*            mysql_ = nullptr;
  std::mutex        mysql_mutex_;
};
} // namespace auth
