#include <string>
#include <utf8.h>

#include <argon2/argon2.h>
#include <DetourNavMesh.h>
#include <fmt/format.h>
#include <boost/asio.hpp>
#include <openssl/evp.h>
#include <hiredis/hiredis.h>
#include <mysql.h>

int main()
{
  std::string text = "hello";
  auto        it   = text.begin();

  utf8::next(it, text.end());

  unsigned char hash[32] = {};
  argon2_hash(2, 1 << 16, 1, "pwd", 3, "salt", 4,
              hash, sizeof(hash), nullptr, 0, Argon2_id, ARGON2_VERSION_NUMBER);

  dtNavMesh* nav = dtAllocNavMesh();
  dtFreeNavMesh(nav);

  const std::string formatted = fmt::format("answer={}", 42);
  (void)formatted;

  boost::asio::io_context io_context;
  (void)io_context;

  unsigned char digest[EVP_MAX_MD_SIZE] = {};
  unsigned int  digest_len             = 0;
  EVP_Digest("abc", 3, digest, &digest_len, EVP_sha256(), nullptr);

  redisContext* redis = redisConnect("127.0.0.1", 6390);
  if (redis)
    redisFree(redis);

  MYSQL* mysql = mysql_init(nullptr);
  mysql_close(mysql);

  return 0;
}
