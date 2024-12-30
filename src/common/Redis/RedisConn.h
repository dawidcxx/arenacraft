#include <hiredis/hiredis.h>

#define sRedisConn RedisConn::instance()

class RedisConn
{
private:
    redisContext *m_redis_ctx;

    RedisConn();
public:
    static RedisConn& instance();
    void init();

    void publishMessage(const char *channel, const char *message);  

};