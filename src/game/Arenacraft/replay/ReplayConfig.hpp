#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace arenacraft::replay
{
// Which arena brackets get recorded. Parsed from Replay.Brackets
// (AC_REPLAY_BRACKETS), a comma separated list of:
//   2v2, 3v3, 5v5   rated team brackets
//   soloq           rated solo queue
//   skirmish        unrated arenas
//   all             everything above
struct Brackets
{
  bool rated2v2 = false;
  bool rated3v3 = false;
  bool rated5v5 = false;
  bool soloq    = false;
  bool skirmish = false;

  bool operator==(Brackets const&) const = default;
};

// What a single arena is, for the bracket filter and for listing.
enum class Bracket : uint8_t
{
  Rated2v2 = 1,
  Rated3v3 = 2,
  Rated5v5 = 3,
  Soloq    = 4,
  Skirmish = 5,
};

struct Config
{
  // Replay.Enabled (AC_REPLAY_ENABLED) - master switch for the replay system:
  // recording and playback. While it is off nothing is recorded and old
  // recordings can no longer be watched.
  bool     enabled = false;
  Brackets brackets{.rated2v2 = true, .rated3v3 = true, .soloq = true};
  // Replay.KeepDays (AC_REPLAY_KEEP_DAYS) - delete recordings older than N days.
  uint32_t keepDays = 0;
  // Replay.MaxTotal (AC_REPLAY_MAX_TOTAL) - keep at most N recordings overall.
  uint32_t maxTotal = 0;
  // Replay.MaxPerPlayer (AC_REPLAY_MAX_PER_PLAYER) - keep at most N recordings
  // per character. A recording is deleted as a whole (for every participant)
  // once it falls out of any participant's newest N.
  uint32_t maxPerPlayer = 0;
  // 0 for every limit above means "no limit".

  [[nodiscard]] bool records(Bracket bracket) const;
};

// Parses a Replay.Brackets value. Unknown tokens are returned through
// `unknown` (first one only) and otherwise ignored.
Brackets parseBrackets(std::string_view value, std::string* unknown = nullptr);

[[nodiscard]] char const*            bracketName(Bracket bracket);
[[nodiscard]] std::optional<Bracket> bracketFromId(uint8_t id);

// Reads the Replay.* options from the server config / environment. Must run on
// the world thread (config reads are not thread safe); see ReplayScripts.cpp.
void          loadConfig();
Config const& config();
} // namespace arenacraft::replay
