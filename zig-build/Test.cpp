// Chcks if vendored (deps/) and system (nix) dependencies are properly loading
// with zig build
#include <string>
#include <typeinfo>
#include <utf8.h>

#include <DetourNavMesh.h>
#include <argon2/argon2.h>
#include <boost/algorithm/string/replace.hpp>
#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/core/demangle.hpp>
#include <boost/filesystem.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/stacktrace.hpp>
#include <boost/system/error_code.hpp>
#include <boost/version.hpp>
#include <fmt/format.h>
#include <hiredis/hiredis.h>
#include <mysql.h>
#include <openssl/evp.h>

// the boost include dirs are filtered in Deps.zig (boost_used_libs); the
// includes above mirror every boost header family src uses, so a
// missing dir fails here before it fails somewhere in the port
int main()
{
  std::string text = "hello";
  auto        it   = text.begin();

  utf8::next(it, text.end());

  unsigned char hash[32] = {};
  argon2_hash(2, 1 << 16, 1, "pwd", 3, "salt", 4, hash, sizeof(hash), nullptr, 0, Argon2_id, ARGON2_VERSION_NUMBER);

  dtNavMesh* nav = dtAllocNavMesh();
  dtFreeNavMesh(nav);

  const std::string formatted = fmt::format("answer={}", 42);
  (void)formatted;

  boost::asio::io_context io_context;
  (void)io_context;

  // pulls boost/date_time through asio's time_traits (see DeadlineTimer.h)
  boost::asio::deadline_timer deadline_timer(io_context);
  (void)deadline_timer;

  boost::filesystem::path fs_path("dir/file.ext");
  (void)fs_path.filename();

  const std::string replaced = boost::algorithm::replace_all_copy(std::string("a-b-c"), "-", "+");
  (void)replaced;

  boost::container::static_vector<int, 4> fixed_vec{1, 2, 3};
  (void)fixed_vec;

  const int answer = boost::lexical_cast<int>("42");
  (void)answer;

  const std::string demangled = boost::core::demangle(typeid(int).name());
  (void)demangled;

  boost::system::error_code ec;
  (void)ec;

  (void)boost::stacktrace::frame();

  auto counting_it = boost::make_counting_iterator(0);
  (void)counting_it;

  (void)BOOST_VERSION;

  unsigned char digest[EVP_MAX_MD_SIZE] = {};
  unsigned int  digest_len              = 0;
  EVP_Digest("abc", 3, digest, &digest_len, EVP_sha256(), nullptr);

  redisContext* redis = redisConnect("127.0.0.1", 6390);
  if (redis)
    redisFree(redis);

  MYSQL* mysql = mysql_init(nullptr);
  mysql_close(mysql);

  return 0;
}
