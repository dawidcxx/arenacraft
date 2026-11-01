#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

struct redisContext;

#define sRedisConn RedisConn::instance()

// Best-effort Redis client used to publish server-side events to external
// consumers (e.g. the soloq matchup event). All connection and I/O work happens
// on a dedicated worker thread; publish() only enqueues a message, so a slow or
// unreachable Redis can never stall the world thread. A Redis outage must never
// block the world: queued publishes are dropped and logged.
class RedisConn
{
public:
  static RedisConn& instance();

  // Reads the Redis.* config and starts the publisher thread. Never throws.
  void init();

  // Enqueues `message` for publication to `channel` and returns immediately.
  // Returns false only when Redis is disabled or the publisher is shutting down;
  // delivery itself is asynchronous and best-effort.
  bool publish(std::string_view channel, std::string_view message);

  // Stops and joins the publisher thread. Safe to call more than once.
  void shutdown();

private:
  RedisConn() = default;
  ~RedisConn();

  RedisConn(RedisConn const&)            = delete;
  RedisConn& operator=(RedisConn const&) = delete;

  struct Message
  {
    std::string channel;
    std::string message;
  };

  void workerLoop();

  // Worker-thread only. The redis context is never touched by other threads.
  bool connect();
  void disconnect();
  bool publishOne(std::string_view channel, std::string_view message);

  std::mutex              m_mutex;
  std::condition_variable m_cv;
  std::deque<Message>     m_queue;
  std::thread             m_worker;
  bool                    m_stop = false;

  redisContext*                         m_context = nullptr;
  bool                                  m_enabled = false;
  std::string                           m_host;
  std::uint16_t                         m_port = 6379;
  std::string                           m_password;
  int                                   m_db = 0;
  std::chrono::steady_clock::time_point m_nextRetry{};
  std::chrono::steady_clock::time_point m_nextDropLog{};
};
