#include "ReplayConfig.hpp"

#include "Config.h"
#include "Log.h"

#include <algorithm>
#include <cctype>

namespace arenacraft::replay
{
namespace
{
Config gConfig;

std::string lowerTrim(std::string_view token)
{
  std::size_t begin = 0;
  std::size_t end   = token.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(token[begin])))
    ++begin;
  while (end > begin && std::isspace(static_cast<unsigned char>(token[end - 1])))
    --end;

  std::string out(token.substr(begin, end - begin));
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
  return out;
}
} // namespace

bool Config::records(Bracket bracket) const
{
  switch (bracket)
  {
  case Bracket::Rated2v2:
    return brackets.rated2v2;
  case Bracket::Rated3v3:
    return brackets.rated3v3;
  case Bracket::Rated5v5:
    return brackets.rated5v5;
  case Bracket::Soloq:
    return brackets.soloq;
  case Bracket::Skirmish:
    return brackets.skirmish;
  }
  return false;
}

Brackets parseBrackets(std::string_view value, std::string* unknown)
{
  Brackets out;
  while (!value.empty())
  {
    std::size_t const comma = value.find(',');
    std::string const token = lowerTrim(value.substr(0, comma));
    value                   = comma == std::string_view::npos ? std::string_view{} : value.substr(comma + 1);

    if (token.empty())
      continue;
    if (token == "2v2" || token == "2s")
      out.rated2v2 = true;
    else if (token == "3v3" || token == "3s")
      out.rated3v3 = true;
    else if (token == "5v5" || token == "5s")
      out.rated5v5 = true;
    else if (token == "soloq")
      out.soloq = true;
    else if (token == "skirmish")
      out.skirmish = true;
    else if (token == "all")
      out = Brackets{true, true, true, true, true};
    else if (unknown && unknown->empty())
      *unknown = token;
  }
  return out;
}

char const* bracketName(Bracket bracket)
{
  switch (bracket)
  {
  case Bracket::Rated2v2:
    return "2v2";
  case Bracket::Rated3v3:
    return "3v3";
  case Bracket::Rated5v5:
    return "5v5";
  case Bracket::Soloq:
    return "soloq";
  case Bracket::Skirmish:
    return "skirmish";
  }
  return "?";
}

std::optional<Bracket> bracketFromId(uint8_t id)
{
  if (id >= static_cast<uint8_t>(Bracket::Rated2v2) && id <= static_cast<uint8_t>(Bracket::Skirmish))
    return static_cast<Bracket>(id);
  return std::nullopt;
}

void loadConfig()
{
  Config cfg;
  cfg.enabled = sConfigMgr->GetOption<bool>("Replay.Enabled", false, false);

  std::string const raw = sConfigMgr->GetOption<std::string>("Replay.Brackets", "2v2,3v3,soloq", false);
  std::string       unknown;
  cfg.brackets = parseBrackets(raw, &unknown);
  if (!unknown.empty())
    LOG_ERROR("server.loading", "Replay.Brackets: unknown bracket '{}' (expected 2v2, 3v3, 5v5, soloq, skirmish, all)",
              unknown);

  cfg.keepDays     = sConfigMgr->GetOption<uint32>("Replay.KeepDays", 0, false);
  cfg.maxTotal     = sConfigMgr->GetOption<uint32>("Replay.MaxTotal", 0, false);
  cfg.maxPerPlayer = sConfigMgr->GetOption<uint32>("Replay.MaxPerPlayer", 0, false);

  gConfig = cfg;
  LOG_INFO("server.loading",
           "Arena replays: recording {} (2v2={} 3v3={} 5v5={} soloq={} skirmish={}), keep days={} total={} per "
           "player={} (0 = unlimited)",
           cfg.enabled ? "on" : "off", cfg.brackets.rated2v2, cfg.brackets.rated3v3, cfg.brackets.rated5v5,
           cfg.brackets.soloq, cfg.brackets.skirmish, cfg.keepDays, cfg.maxTotal, cfg.maxPerPlayer);
}

Config const& config() { return gConfig; }
} // namespace arenacraft::replay
