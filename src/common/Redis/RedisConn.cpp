#include "RedisConn.h"

#include "Config.h"
#include "Log.h"

#include <hiredis/hiredis.h>

namespace
{
// A failed connect is retried at most this often, so a down Redis cannot stall
// the world thread once per published event.
constexpr std::chrono::seconds ConnectRetryInterval{10};
timeval const                  ConnectTimeout{1, 0};
} // namespace

RedisConn& RedisConn::instance()
{
  static RedisConn instance;
  return instance;
}

RedisConn::~RedisConn()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  disconnectLocked();
}

void RedisConn::disconnectLocked()
{
  if (m_context)
  {
    redisFree(m_context);
    m_context = nullptr;
  }
}

bool RedisConn::connectLocked()
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
    LOG_WARN("server", "Redis: cannot connect to {}:{} ({}); disabling until retry.", m_host, m_port,
             m_context ? m_context->errstr : "context allocation failed");
    disconnectLocked();
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
      disconnectLocked();
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
      disconnectLocked();
      m_nextRetry = now + ConnectRetryInterval;
      return false;
    }
  }

  LOG_INFO("server", "Redis: connected to {}:{}.", m_host, m_port);
  return true;
}

void RedisConn::init()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  disconnectLocked();

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

  connectLocked();
}

bool RedisConn::publish(std::string_view channel, std::string_view message)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!connectLocked())
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
    disconnectLocked();
    return false;
  }

  bool const ok = reply->type == REDIS_REPLY_INTEGER;
  if (!ok)
    LOG_WARN("server", "Redis: unexpected reply while publishing to '{}'.", channel);
  freeReplyObject(reply);
  return ok;
}
