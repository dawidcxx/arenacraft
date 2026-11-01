#include "RedisConn.h"

#include "Config.h"
#include "Log.h"

#include <hiredis/hiredis.h>

namespace
{
// A failed connect is retried at most this often, so a down Redis cannot make
// the publisher thread spin.
constexpr std::chrono::seconds ConnectRetryInterval{10};

// Bounded queue: event publishing is best-effort and must never grow without
// limit if the consumer is unreachable. When full, the oldest message is
// dropped so consumers still see the most recent state.
constexpr std::size_t MaxQueuedMessages = 1024;

// Do not spam the log when the queue overflows during an outage.
constexpr std::chrono::seconds DropLogInterval{10};

timeval const ConnectTimeout{1, 0};
} // namespace

RedisConn& RedisConn::instance()
{
  static RedisConn instance;
  return instance;
}

RedisConn::~RedisConn() { shutdown(); }

void RedisConn::disconnect()
{
  if (m_context)
  {
    redisFree(m_context);
    m_context = nullptr;
  }
}

bool RedisConn::connect()
{
  if (m_context)
    return true;
  if (!m_enabled)
    return false;

  auto const now = std::chrono::steady_clock::now();
  if (now < m_nextRetry)
    return false;

  m_context = redisConnectWithTimeout(m_host.c_str(), m_port, ConnectTimeout);
  if (!m_context || m_context->err)
  {
    LOG_WARN("server", "Redis: cannot connect to {}:{} ({}); retrying later.", m_host, m_port,
             m_context ? m_context->errstr : "context allocation failed");
    disconnect();
    m_nextRetry = now + ConnectRetryInterval;
    return false;
  }

  if (!m_password.empty())
  {
    redisReply* reply = static_cast<redisReply*>(redisCommand(m_context, "AUTH %s", m_password.c_str()));
    bool const  ok    = reply && reply->type != REDIS_REPLY_ERROR;
    if (!ok)
      LOG_WARN("server", "Redis: AUTH failed: {}", reply ? reply->str : "no reply");
    if (reply)
      freeReplyObject(reply);
    if (!ok)
    {
      disconnect();
      m_nextRetry = now + ConnectRetryInterval;
      return false;
    }
  }

  if (m_db != 0)
  {
    redisReply* reply = static_cast<redisReply*>(redisCommand(m_context, "SELECT %d", m_db));
    bool const  ok    = reply && reply->type != REDIS_REPLY_ERROR;
    if (!ok)
      LOG_WARN("server", "Redis: SELECT {} failed: {}", m_db, reply ? reply->str : "no reply");
    if (reply)
      freeReplyObject(reply);
    if (!ok)
    {
      disconnect();
      m_nextRetry = now + ConnectRetryInterval;
      return false;
    }
  }

  LOG_INFO("server", "Redis: connected to {}:{}.", m_host, m_port);
  return true;
}

bool RedisConn::publishOne(std::string_view channel, std::string_view message)
{
  if (!connect())
    return false;

  // redisCommandArgv keeps the payload binary-safe (no format expansion), so
  // JSON containing spaces, quotes or '%' is published verbatim.
  char const* argv[3]    = {"PUBLISH", channel.data(), message.data()};
  std::size_t argvlen[3] = {7, channel.size(), message.size()};
  redisReply* reply      = static_cast<redisReply*>(redisCommandArgv(m_context, 3, argv, argvlen));
  if (!reply)
  {
    LOG_WARN("server", "Redis: publish to '{}' failed ({}); reconnecting on the next event.", channel,
             m_context ? m_context->errstr : "no context");
    disconnect();
    m_nextRetry = std::chrono::steady_clock::now() + ConnectRetryInterval;
    return false;
  }

  bool const ok = reply->type == REDIS_REPLY_INTEGER;
  if (!ok)
    LOG_WARN("server", "Redis: unexpected reply while publishing to '{}'.", channel);
  freeReplyObject(reply);
  return ok;
}

void RedisConn::workerLoop()
{
  std::unique_lock<std::mutex> lock(m_mutex);
  while (!m_stop)
  {
    if (m_queue.empty())
    {
      m_cv.wait(lock, [this] { return m_stop || !m_queue.empty(); });
      continue;
    }

    // Keep messages queued while Redis is down instead of discarding them one
    // by one; wait until the retry window opens, then connect off-lock.
    if (!m_context)
    {
      if (std::chrono::steady_clock::now() < m_nextRetry)
      {
        m_cv.wait_until(lock, m_nextRetry, [this] { return m_stop; });
        continue;
      }

      lock.unlock();
      bool const connected = connect();
      lock.lock();
      if (m_stop)
        break;
      if (!connected)
        continue;
    }

    Message message = std::move(m_queue.front());
    m_queue.pop_front();

    // Perform the blocking network I/O without holding the queue lock so
    // publish() from game threads stays non-blocking.
    lock.unlock();
    publishOne(message.channel, message.message);
    lock.lock();
  }

  m_queue.clear();
  lock.unlock();
  disconnect();
}

void RedisConn::init()
{
  shutdown();

  std::lock_guard<std::mutex> lock(m_mutex);
  m_enabled   = sConfigMgr->GetOption<bool>("Redis.Enabled", true);
  m_host      = sConfigMgr->GetOption<std::string>("Redis.Host", "127.0.0.1");
  m_port      = static_cast<std::uint16_t>(sConfigMgr->GetOption<int32>("Redis.Port", 6379));
  m_password  = sConfigMgr->GetOption<std::string>("Redis.Password", "");
  m_db        = sConfigMgr->GetOption<int32>("Redis.Db", 0);
  m_nextRetry = {};

  if (!m_enabled)
  {
    LOG_INFO("server", "Redis: disabled by config.");
    return;
  }

  m_stop   = false;
  m_worker = std::thread(&RedisConn::workerLoop, this);
}

bool RedisConn::publish(std::string_view channel, std::string_view message)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_enabled || m_stop)
    return false;

  if (m_queue.size() >= MaxQueuedMessages)
  {
    m_queue.pop_front();
    auto const now = std::chrono::steady_clock::now();
    if (now >= m_nextDropLog)
    {
      LOG_WARN("server", "Redis: publish queue full ({}); dropping oldest events.", MaxQueuedMessages);
      m_nextDropLog = now + DropLogInterval;
    }
  }

  m_queue.push_back(Message{std::string(channel), std::string(message)});
  m_cv.notify_one();
  return true;
}

void RedisConn::shutdown()
{
  std::thread worker;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_worker.joinable())
      return;
    m_stop = true;
    m_cv.notify_all();
    worker = std::move(m_worker);
  }
  worker.join();
}
