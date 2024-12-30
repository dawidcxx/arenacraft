#include "RedisConn.h"
#include <hiredis/hiredis.h>
#include <iostream>
#include <stdexcept>

RedisConn& RedisConn::instance()
{
    static RedisConn instance;
    return instance;
}

RedisConn::RedisConn() {}

// Initialization method
void RedisConn::init()
{
    // Example: Establish a connection to the Redis server
    const char *hostname = "127.0.0.1";
    int port = 6379;
    redisContext *context = redisConnect(hostname, port);

    if (context == nullptr || context->err)
    {
        throw std::runtime_error("Failed to connect to Redis: " + std::string(context->errstr));
    }
    std::cout << "Connected to Redis" << std::endl;
    this->m_redis_ctx = context;
}

void RedisConn::publishMessage(const char *channel, const char *message)
{
    // Example: Publish a message to a channel
    redisReply *reply = (redisReply *)redisCommand(m_redis_ctx, "PUBLISH %s %s", channel, message);
    if (reply == nullptr)
    {
        LOG_WARN("redis", "Failed to publish message to Redis");
    }
    freeReplyObject(reply);
}
