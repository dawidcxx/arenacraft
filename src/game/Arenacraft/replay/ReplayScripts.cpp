/*
 * Arena replays: config/startup hooks and the player commands.
 *
 *   .replay <id>          watch recording <id> (alias: .replay watch <id>)
 *   .replay list [name]   newest recordings, optionally only those with <name>
 *   .replay leave         stop watching
 *   .replay pause | resume                   pause / resume the replay you watch
 *   .replay seek <s|m:ss|+N|-N>              jump on its timeline (forward or back)
 *   .replay state                            send the addon state message now (ACREPLAY, see ai-doc/replay.md)
 *   .replay report <id> <name> <category>   report a player of a match you played in (see Patrol.hpp)
 */
#include "ReplayScripts.hpp"

#include "ArenaSpectator.h"
#include "Chat.h"
#include "CommandScript.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "ObjectMgr.h"
#include "Patrol.hpp"
#include "Player.h"
#include "ReplayConfig.hpp"
#include "ReplayPlayback.hpp"
#include "ReplayRecorder.hpp"
#include "ReplayTimeline.hpp"
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
        {"list", HandleList, SEC_PLAYER, Console::Yes},  {"report", HandleReport, SEC_PLAYER, Console::No},
        {"leave", HandleLeave, SEC_PLAYER, Console::No}, {"watch", HandleWatch, SEC_PLAYER, Console::No},
        {"pause", HandlePause, SEC_PLAYER, Console::No}, {"resume", HandleResume, SEC_PLAYER, Console::No},
        {"seek", HandleSeek, SEC_PLAYER, Console::No},   {"state", HandleState, SEC_PLAYER, Console::No},
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
    if (patrol::IsReviewing(player->GetGUID()))
    {
      handler->SendSysMessage("Finish or skip your patrol case first.");
      return true;
    }
    watch(player, id);
    return true;
  }

  static std::string clock(uint32 ms)
  {
    uint32 const seconds = ms / IN_MILLISECONDS;
    return Acore::StringFormat("{}:{:02}", seconds / 60, seconds % 60);
  }

  static std::shared_ptr<Playback> watchedBy(ChatHandler* handler, Player*& player)
  {
    player                             = handler->GetPlayer();
    std::shared_ptr<Playback> playback = player ? playbackOf(player->GetGUID()) : nullptr;
    if (!playback)
      handler->SendSysMessage("You are not watching a replay.");
    return playback;
  }

  static bool HandlePause(ChatHandler* handler)
  {
    Player* player = nullptr;
    if (std::shared_ptr<Playback> playback = watchedBy(handler, player))
    {
      if (playback->setPaused(player, true))
        handler->PSendSysMessage("Replay paused at {} / {}.", clock(playback->positionMs()),
                                 clock(playback->durationMs()));
      else
        handler->SendSysMessage("The replay has not started yet.");
    }
    return true;
  }

  static bool HandleResume(ChatHandler* handler)
  {
    Player* player = nullptr;
    if (std::shared_ptr<Playback> playback = watchedBy(handler, player))
    {
      if (playback->setPaused(player, false))
        handler->PSendSysMessage("Replay resumed at {} / {}.", clock(playback->positionMs()),
                                 clock(playback->durationMs()));
      else
        handler->SendSysMessage("The replay has not started yet.");
    }
    return true;
  }

  static bool HandleSeek(ChatHandler* handler, Tail target)
  {
    Player*                   player   = nullptr;
    std::shared_ptr<Playback> playback = watchedBy(handler, player);
    if (!playback)
      return true;

    std::optional<uint32_t> const ms = parseSeekTarget(target, playback->positionMs(), playback->durationMs());
    if (!ms)
    {
      handler->SendSysMessage("Usage: .replay seek <seconds | m:ss | +seconds | -seconds>");
      return true;
    }
    if (!playback->seek(player, *ms))
    {
      handler->SendSysMessage("The replay has not started yet.");
      return true;
    }
    handler->PSendSysMessage("Replay at {} / {}{}.", clock(playback->positionMs()), clock(playback->durationMs()),
                             playback->paused() ? " (paused)" : "");
    return true;
  }

  static bool HandleState(ChatHandler* handler)
  {
    Player* player = nullptr;
    if (std::shared_ptr<Playback> playback = watchedBy(handler, player))
      playback->sendState(player);
    return true;
  }

  static bool HandleReport(ChatHandler* handler, uint32 id, std::string suspect, std::string category)
  {
    Player* player = handler->GetPlayer();
    if (!player)
      return false;
    patrol::Report(handler, player, id, suspect, category);
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
    if (playbackOf(player->GetGUID()))
      ArenaSpectator::SendPacketTo(player, std::string("ACREPLAY\tEND;"));
    player->TeleportToEntryPoint();
    return true;
  }

  static bool HandleList(ChatHandler* handler, Optional<std::string> name)
  {
    // a patroller could match the match they review to its real names
    if (Player* player = handler->GetPlayer(); player && patrol::IsReviewing(player->GetGUID()))
    {
      handler->SendSysMessage("Finish or skip your patrol case first.");
      return true;
    }

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
