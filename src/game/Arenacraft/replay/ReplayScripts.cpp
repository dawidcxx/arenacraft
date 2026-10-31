/*
 * Arena replays: config/startup hooks and the player commands.
 *
 *   .replay <id>          watch recording <id> (alias: .replay watch <id>)
 *   .replay list [name]   newest recordings, optionally only those with <name>
 *   .replay leave         stop watching
 */
#include "ReplayScripts.hpp"

#include "Chat.h"
#include "CommandScript.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ReplayConfig.hpp"
#include "ReplayPlayback.hpp"
#include "ReplayRecorder.hpp"
#include "WorldScript.h"

#include <sstream>

using namespace Acore::ChatCommands;

namespace arenacraft::replay
{
namespace
{
constexpr uint32 ListLimit = 10;

class ReplayWorldScript : public WorldScript
{
public:
  ReplayWorldScript()
      : WorldScript("arenacraft::replay::ReplayWorldScript", {WORLDHOOK_ON_AFTER_CONFIG_LOAD, WORLDHOOK_ON_STARTUP})
  {
  }

  void OnAfterConfigLoad(bool /*reload*/) override { loadConfig(); }

  void OnStartup() override
  {
    loadConfig();
    ReplayRecorder::instance().init();
  }
};

std::string mapName(uint32 mapId)
{
  if (MapEntry const* entry = sMapStore.LookupEntry(mapId))
    return entry->name[0];
  return std::to_string(mapId);
}

class ReplayCommandScript : public CommandScript
{
public:
  ReplayCommandScript() : CommandScript("arenacraft::replay::ReplayCommandScript") {}

  ChatCommandTable GetCommands() const override
  {
    static ChatCommandTable replayTable = {
        {"list", HandleList, SEC_PLAYER, Console::Yes},
        {"leave", HandleLeave, SEC_PLAYER, Console::No},
        {"watch", HandleWatch, SEC_PLAYER, Console::No},
        {"", HandleWatch, SEC_PLAYER, Console::No},
    };
    static ChatCommandTable commandTable = {{"replay", replayTable}};
    return commandTable;
  }

  static bool HandleWatch(ChatHandler* handler, uint32 id)
  {
    Player* player = handler->GetPlayer();
    if (!player)
      return false;
    watch(player, id);
    return true;
  }

  static bool HandleLeave(ChatHandler* handler)
  {
    Player* player = handler->GetPlayer();
    if (!player || !player->IsSpectator() || !player->FindMap() || !player->FindMap()->IsBattleArena())
    {
      handler->SendSysMessage("You are not watching a replay.");
      return true;
    }
    player->TeleportToEntryPoint();
    return true;
  }

  static bool HandleList(ChatHandler* handler, Optional<std::string> name)
  {
    CharacterDatabasePreparedStatement* stmt;
    if (name && !name->empty())
    {
      std::string normalized = *name;
      if (!normalizePlayerName(normalized))
      {
        handler->SendErrorMessage("Invalid character name.", false);
        return false;
      }
      stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ARENA_REPLAYS_BY_NAME);
      stmt->SetData(0, normalized);
      stmt->SetData(1, ListLimit);
    }
    else
    {
      stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ARENA_REPLAYS);
      stmt->SetData(0, ListLimit);
    }

    PreparedQueryResult result = CharacterDatabase.Query(stmt);
    if (!result)
    {
      handler->SendSysMessage("No replays found.");
      return true;
    }

    handler->SendSysMessage("Replays (newest first) - watch with .replay <id>:");
    do
    {
      Field*                 fields   = result->Fetch();
      uint32 const           id       = fields[0].Get<uint32>();
      std::optional<Bracket> bracket  = bracketFromId(fields[1].Get<uint8>());
      uint32 const           duration = fields[3].Get<uint32>() / IN_MILLISECONDS;
      uint8 const            winner   = fields[4].Get<uint8>();

      std::ostringstream line;
      line << "#" << id << " " << (bracket ? bracketName(*bracket) : "?") << " " << mapName(fields[2].Get<uint32>())
           << " " << duration / 60 << "m" << duration % 60 << "s " << fields[5].Get<std::string>() << " | "
           << fields[6].Get<std::string>() << " vs " << fields[7].Get<std::string>();
      if (winner == TEAM_ALLIANCE)
        line << " (first team won)";
      else if (winner == TEAM_HORDE)
        line << " (second team won)";
      handler->SendSysMessage(line.str());
    } while (result->NextRow());
    return true;
  }
};
} // namespace

void AddReplayScripts()
{
  new ReplayWorldScript();
  new ReplayCommandScript();
}
} // namespace arenacraft::replay
