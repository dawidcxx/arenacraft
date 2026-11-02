#pragma once

#include "PatrolRules.hpp"

#include <cstdint>
#include <string>

// Patrol.* options (environment: AC_PATROL_*, see .env.example and ai-doc/patrol.md). Read on the world thread at
// startup and on config reload.
namespace arenacraft::patrol
{
enum class Punishment : uint8_t
{
  BanAccount,   // the whole account of the suspect (BanMgr::BanAccountByPlayerName)
  BanCharacter, // only the reported character (BanMgr::BanCharacter)
  None,         // only mark the case guilty; staff decide (.patrol cases)
};

struct Config
{
  // Patrol.Enabled (AC_PATROL_ENABLED) - master switch for reports and reviews. Needs replays enabled too.
  bool enabled = false;

  // --- who may review ---
  // Patrol.MinGames (AC_PATROL_MIN_GAMES) - solo queue games this season a patroller needs.
  uint32_t minGames = 50;
  // Patrol.RatingWindow (AC_PATROL_RATING_WINDOW) - a patroller's solo queue rating must be within +-N of the
  // suspect's rating at report time. 0 = any rating.
  uint32_t ratingWindow = 0;
  // Patrol.StaffExempt (AC_PATROL_STAFF_EXEMPT) - staff accounts skip the games / rating requirements.
  bool staffExempt = true;

  // --- reports ---
  // Patrol.Categories (AC_PATROL_CATEGORIES) - categories that can be reported and are asked about:
  // cheating, automation, griefing, wintrading, all.
  uint8_t categories = AllCategories;
  // Patrol.MaxReportsPerDay (AC_PATROL_MAX_REPORTS_PER_DAY) - per reporting account. 0 = unlimited.
  uint32_t maxReportsPerDay = 10;
  // Patrol.ReportWindowHours (AC_PATROL_REPORT_WINDOW_HOURS) - only matches at most N hours old can be reported.
  // 0 = any age (as long as the replay exists).
  uint32_t reportWindowHours = 0;

  // --- decision ---
  // Patrol.Verdicts (AC_PATROL_VERDICTS) - verdicts that close a case.
  uint32_t verdicts = 5;
  // Patrol.YesToBan (AC_PATROL_YES_TO_BAN) - yes votes in one category that make the suspect guilty of it.
  uint32_t yesToBan = 4;
  // Patrol.Punishment (AC_PATROL_PUNISHMENT) - what a guilty verdict does: account | character | none.
  Punishment punishment = Punishment::BanAccount;
  // Patrol.BanDuration (AC_PATROL_BAN_DURATION) - BanMgr duration: 30m, 12h, 7d, 1d12h, ... ("0" = permanent).
  std::string banDuration = "7d";
};

void          loadConfig();
Config const& config();

// "ban account for 7d", "ban character permanently", "no punishment (flag for staff)"
std::string describePunishment(Config const& cfg);
} // namespace arenacraft::patrol
