/*
 * `ac db-update` - apply pending data/sql/updates files to the acore databases.
 *
 * Connection settings come from the same environment variables the servers use
 * (AC_LOGIN_DATABASE_INFO, AC_CHARACTER_DATABASE_INFO, AC_WORLD_DATABASE_INFO),
 * loaded from ./.env by default or an explicit --env-file.
 */

#include "Config.h"
#include "DatabaseUpdater.h"
#include "Log.h"
#include "MySQLThreading.h"
#include <argparse/argparse.hpp>
#include <filesystem>
#include <fmt/format.h>
#include <iostream>
#include <string>
#include <vector>

namespace
{
struct Target
{
  char const* name;
  char const* configKey;
  char const* sqlDirectory;
};

Target const TARGETS[] = {
    {"auth", "LoginDatabaseInfo", "db_auth"},
    {"characters", "CharacterDatabaseInfo", "db_characters"},
    {"world", "WorldDatabaseInfo", "db_world"},
};

std::vector<std::string> ParseDatabaseList(std::string const& value, bool& valid)
{
  std::vector<std::string> result;
  std::size_t              start = 0;
  valid                          = true;

  while (start <= value.size())
  {
    std::size_t end   = value.find(',', start);
    std::string token = value.substr(start, end == std::string::npos ? std::string::npos : end - start);

    std::size_t first = token.find_first_not_of(" \t");
    std::size_t last  = token.find_last_not_of(" \t");
    if (first != std::string::npos)
      token = token.substr(first, last - first + 1);
    else
      token.clear();

    if (!token.empty())
    {
      bool known = false;
      for (Target const& target : TARGETS)
        known = known || token == target.name;

      if (!known)
      {
        fmt::print(stderr, "unknown database '{}' (expected auth, characters, world)\n", token);
        valid = false;
      }
      else
        result.push_back(token);
    }

    if (end == std::string::npos)
      break;

    start = end + 1;
  }

  return result;
}
} // namespace

int db_update_main(int argc, char** argv)
{
  argparse::ArgumentParser parser("ac db-update", "1.0");
  parser.add_description("Apply pending data/sql/updates files to the acore databases");
  parser.add_argument("-e", "--env-file")
      .help("load environment variables from a KEY=VALUE file (default: ./.env if present)")
      .default_value(std::string(""));
  parser.add_argument("--sql-dir")
      .help("sql root directory (default: AC_SQL_DIR or <DataDir>/sql)")
      .default_value(std::string(""));
  parser.add_argument("--db")
      .help("comma separated subset of auth,characters,world")
      .default_value(std::string("auth,characters,world"));
  parser.add_argument("--dry-run").help("show what would be applied without executing").flag();

  try
  {
    parser.parse_args(argc, argv);
  }
  catch (std::exception const& error)
  {
    std::cerr << error.what() << std::endl;
    return 1;
  }

  std::string const envFile = parser.get<std::string>("--env-file");
  if (!envFile.empty())
  {
    if (!sConfigMgr->ApplyEnvFile(envFile))
    {
      fmt::print(stderr, "Failed to open env file '{}'\n", envFile);
      return 1;
    }
  }
  else
  {
    sConfigMgr->ApplyEnvFile(".env");
  }

  sConfigMgr->Configure();
  if (!sConfigMgr->LoadAppConfigs())
    return 1;

  sLog->Initialize(nullptr);

  bool                           valid    = false;
  std::vector<std::string> const selected = ParseDatabaseList(parser.get<std::string>("--db"), valid);
  if (!valid || selected.empty())
    return 1;

  std::string sqlDir = parser.get<std::string>("--sql-dir");
  if (sqlDir.empty())
    sqlDir = sConfigMgr->GetOption<std::string>("SqlDir", "");
  if (sqlDir.empty())
    sqlDir = sConfigMgr->GetDataPath() + "/sql";

  std::filesystem::path const updatesRoot = std::filesystem::path(sqlDir) / "updates";
  bool const                  dryRun      = parser.get<bool>("--dry-run");

  if (!std::filesystem::is_directory(updatesRoot))
  {
    fmt::print(stderr, "updates directory not found: {}\n", updatesRoot.string());
    return 1;
  }

  MySQL::Library_Init();

  bool success = true;
  for (std::string const& name : selected)
  {
    Target const* target = nullptr;
    for (Target const& candidate : TARGETS)
    {
      if (name == candidate.name)
      {
        target = &candidate;
        break;
      }
    }

    std::filesystem::path const updateDirectory = updatesRoot / target->sqlDirectory;
    if (!std::filesystem::is_directory(updateDirectory))
    {
      fmt::print(stderr, "[{}] updates directory not found: {}\n", target->name, updateDirectory.string());
      success = false;
      continue;
    }

    std::string const connectionInfo = sConfigMgr->GetOption<std::string>(target->configKey, "");
    if (connectionInfo.empty())
    {
      fmt::print(stderr, "[{}] {} is not set\n", target->name, target->configKey);
      success = false;
      continue;
    }

    Acore::Database::DatabaseUpdateResult result;
    if (!Acore::Database::ApplyDatabaseUpdates(target->name, connectionInfo, updateDirectory, dryRun, result))
    {
      success = false;
      continue;
    }

    fmt::print("[{}] {}: {} applied, {} already recorded", target->name, dryRun ? "dry run" : "done", result.applied,
               result.skipped);
    if (result.hashMismatches > 0)
      fmt::print(", {} hash mismatches", result.hashMismatches);
    fmt::print("\n");
  }

  MySQL::Library_End();
  return success ? 0 : 1;
}
