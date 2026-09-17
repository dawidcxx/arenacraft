#include "RedisConn.h"
#include "Config.h"
#include <hiredis/hiredis.h>
#include <iostream>
#include <stdexcept>
#include "Log.h"

RedisConn& RedisConn::instance()
{
  static RedisConn instance;
  return instance;
}

RedisConn::RedisConn() {}

// Initialization method
void RedisConn::init()
{
  // overridable via AC_REDIS_HOST / AC_REDIS_PORT
  std::string const hostname = sConfigMgr->GetOption<std::string>("Redis.Host", "127.0.0.1");
  int const         port     = sConfigMgr->GetOption<int32>("Redis.Port", 6379);
  redisContext*     context  = redisConnect(hostname.c_str(), port);

  if (context == nullptr || context->err)
  {
    throw std::runtime_error("Failed to connect to Redis at " + hostname + ":" + std::to_string(port) + ": " +
                             std::string(context ? context->errstr : "context allocation failed"));
  }
  std::cout << "Connected to Redis at " << hostname << ":" << port << std::endl;
  this->m_redis_ctx = context;
}

void RedisConn::publishMessage(const char* channel, const char* message)
{
  // Example: Publish a message to a channel
  redisReply* reply = (redisReply*)redisCommand(m_redis_ctx, "PUBLISH %s %s", channel, message);
  if (reply == nullptr)
  {
    LOG_WARN("redis", "Failed to publish message to Redis");
  }
  freeReplyObject(reply);
}
