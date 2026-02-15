#include "../Include/Logging.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>

namespace logging
{
namespace
{
constexpr std::string_view to_string(const Level level)
{
  switch (level)
  {
    case Level::info:
      return "INFO";
    case Level::error:
      return "ERROR";
  }

  return "UNKNOWN";
}
} // namespace

void write(const Level level, const std::string_view message, const std::source_location where)
{
  static std::mutex log_mutex;

  const auto now    = std::chrono::system_clock::now();
  const auto now_tt = std::chrono::system_clock::to_time_t(now);

  std::tm local_tm{};
  localtime_r(&now_tt, &local_tm);

  const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

  std::lock_guard<std::mutex> lock(log_mutex);
  auto& out = (level == Level::error) ? std::cerr : std::cout;
  out << '[' << to_string(level) << "] "
      << std::put_time(&local_tm, "%F %T") << '.' << std::setw(3) << std::setfill('0') << millis << ' '
      << where.function_name() << ':' << where.line() << " - " << message << '\n'
      << std::flush;
}
} // namespace logging
