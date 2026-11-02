#include "PatrolConfig.hpp"

#include "Config.h"
#include "Log.h"

#include <algorithm>
#include <cctype>

namespace arenacraft::patrol
{
namespace
{
Config gConfig;

std::string lower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });
  return value;
}
} // namespace

void loadConfig()
{
  Config cfg;
  cfg.enabled      = sConfigMgr->GetOption<bool>("Patrol.Enabled", false, false);
  cfg.minGames     = sConfigMgr->GetOption<uint32>("Patrol.MinGames", cfg.minGames, false);
  cfg.ratingWindow = sConfigMgr->GetOption<uint32>("Patrol.RatingWindow", cfg.ratingWindow, false);
  cfg.staffExempt  = sConfigMgr->GetOption<bool>("Patrol.StaffExempt", cfg.staffExempt, false);

  std::string unknown;
  cfg.categories = parseCategoryMask(sConfigMgr->GetOption<std::string>("Patrol.Categories", "all", false), &unknown);
  if (!unknown.empty())
    LOG_ERROR("server.loading",
              "Patrol.Categories: unknown category '{}' (expected cheating, automation, griefing, wintrading, all)",
              unknown);
  if (!cfg.categories)
  {
    LOG_ERROR("server.loading", "Patrol.Categories selects no category; using all");
    cfg.categories = AllCategories;
  }

  cfg.maxReportsPerDay  = sConfigMgr->GetOption<uint32>("Patrol.MaxReportsPerDay", cfg.maxReportsPerDay, false);
  cfg.reportWindowHours = sConfigMgr->GetOption<uint32>("Patrol.ReportWindowHours", cfg.reportWindowHours, false);

  cfg.verdicts = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("Patrol.Verdicts", cfg.verdicts, false));
  cfg.yesToBan = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("Patrol.YesToBan", cfg.yesToBan, false));
  if (cfg.yesToBan > cfg.verdicts)
  {
    LOG_ERROR("server.loading", "Patrol.YesToBan ({}) is above Patrol.Verdicts ({}); nobody could ever be banned",
              cfg.yesToBan, cfg.verdicts);
  }
  cfg.banDuration = sConfigMgr->GetOption<std::string>("Patrol.BanDuration", cfg.banDuration, false);

  std::string const punishment = lower(sConfigMgr->GetOption<std::string>("Patrol.Punishment", "account", false));
  if (punishment == "character")
    cfg.punishment = Punishment::BanCharacter;
  else if (punishment == "none")
    cfg.punishment = Punishment::None;
  else if (punishment != "account")
    LOG_ERROR("server.loading", "Patrol.Punishment: unknown value '{}' (expected account, character or none)",
              punishment);

  gConfig = cfg;
  LOG_INFO("server.loading",
           "Patrol: {} (categories: {}; reviewers: {}+ games, rating window {}; {} verdicts, guilty at {} yes -> {}; "
           "{} reports/day, report window {}h)",
           cfg.enabled ? "on" : "off", labels(cfg.categories), cfg.minGames, cfg.ratingWindow, cfg.verdicts,
           cfg.yesToBan, describePunishment(cfg), cfg.maxReportsPerDay, cfg.reportWindowHours);
}

Config const& config() { return gConfig; }

std::string describePunishment(Config const& cfg)
{
  if (cfg.punishment == Punishment::None)
    return "no punishment (flag for staff)";
  std::string const target = cfg.punishment == Punishment::BanAccount ? "account" : "character";
  if (cfg.banDuration.empty() || cfg.banDuration == "0")
    return "ban " + target + " permanently";
  return "ban " + target + " for " + cfg.banDuration;
}
} // namespace arenacraft::patrol
