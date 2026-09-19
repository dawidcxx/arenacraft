#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

struct redisContext;

#define sRedisConn RedisConn::instance()

// Best-effort, thread-safe Redis client used to publish server-side events to
// external consumers (e.g. the soloq matchup event). A Redis outage must never
// block the world: publishes become no-ops and are logged.
class RedisConn
{
public:
  static RedisConn& instance();

  // Reads the Redis.* config and attempts an initial connection. Never throws.
  void init();

  // Publishes `message` to `channel`. Returns true when Redis acknowledged it.
  // Safe to call from any thread; no-ops when Redis is disabled or unreachable.
  bool publish(std::string_view channel, std::string_view message);

private:
  RedisConn() = default;
  ~RedisConn();

  RedisConn(RedisConn const&)            = delete;
  RedisConn& operator=(RedisConn const&) = delete;

  // m_mutex must be held by the caller.
  bool connectLocked();
  void disconnectLocked();

  std::mutex                            m_mutex;
  redisContext*                         m_context = nullptr;
  bool                                  m_enabled = false;
  std::string                           m_host;
  std::uint16_t                         m_port = 6379;
  std::string                           m_password;
  int                                   m_db = 0;
  std::chrono::steady_clock::time_point m_nextRetry{};
};
