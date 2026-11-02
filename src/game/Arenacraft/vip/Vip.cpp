/*
 * VIP characters.
 *
 *   .vip add [name]   flag the character (or the selected player) as VIP
 *   .vip del [name]   remove the flag
 *   .vip get [name]   show whether the character is VIP
 */
#include "Vip.hpp"

#include "Chat.h"
#include "CommandScript.h"
#include "DatabaseEnv.h"
#include "Language.h"
#include "Log.h"
#include "Player.h"
#include "Timer.h"
#include "WorldScript.h"

#include <mutex>
#include <shared_mutex>
#include <unordered_set>

using namespace Acore::ChatCommands;

namespace arenacraft::vip
{
namespace
{
// Read from map threads (transmog gossip), written from the world thread (commands, startup).
std::shared_mutex                       CacheMutex;
std::unordered_set<ObjectGuid::LowType> Cache;
} // namespace

void Load()
{
  uint32 const oldMSTime = getMSTime();

  std::unordered_set<ObjectGuid::LowType> loaded;
  if (QueryResult result = CharacterDatabase.Query("SELECT guid FROM character_vip"))
  {
    do
    {
      loaded.insert((*result)[0].Get<uint32>());
    } while (result->NextRow());
  }

  std::size_t const count = loaded.size();
  {
    std::unique_lock lock(CacheMutex);
    Cache = std::move(loaded);
  }

  LOG_INFO("server.loading", ">> Loaded {} VIP characters in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

bool IsVip(ObjectGuid guid)
{
  std::shared_lock lock(CacheMutex);
  return Cache.contains(guid.GetCounter());
}

bool IsVip(Player const* player) { return player && IsVip(player->GetGUID()); }

bool Set(ObjectGuid guid, bool vip)
{
  ObjectGuid::LowType const low = guid.GetCounter();
  {
    std::unique_lock lock(CacheMutex);
    bool const       changed = vip ? Cache.insert(low).second : Cache.erase(low) > 0;
    if (!changed)
      return false;
  }

  if (vip)
    CharacterDatabase.Execute("REPLACE INTO character_vip (guid) VALUES ({})", low);
  else
    CharacterDatabase.Execute("DELETE FROM character_vip WHERE guid = {}", low);
  return true;
}

namespace
{
class VipWorldScript : public WorldScript
{
public:
  VipWorldScript() : WorldScript("arenacraft::vip::VipWorldScript", {WORLDHOOK_ON_STARTUP}) {}

  void OnStartup() override { Load(); }
};

class VipCommandScript : public CommandScript
{
public:
  VipCommandScript() : CommandScript("arenacraft::vip::VipCommandScript") {}

  ChatCommandTable GetCommands() const override
  {
    static ChatCommandTable vipTable = {
        {"add", HandleAdd, SEC_ADMINISTRATOR, Console::Yes},
        {"del", HandleDel, SEC_ADMINISTRATOR, Console::Yes},
        {"get", HandleGet, SEC_ADMINISTRATOR, Console::Yes},
    };
    static ChatCommandTable commandTable = {{"vip", vipTable}};
    return commandTable;
  }

  static Optional<PlayerIdentifier> Resolve(ChatHandler* handler, Optional<PlayerIdentifier> player)
  {
    if (!player)
      player = PlayerIdentifier::FromTargetOrSelf(handler);
    if (!player)
      handler->SendErrorMessage(LANG_PLAYER_NOT_FOUND);
    return player;
  }

  static bool HandleAdd(ChatHandler* handler, Optional<PlayerIdentifier> player)
  {
    player = Resolve(handler, player);
    if (!player)
      return false;

    if (Set(player->GetGUID(), true))
      handler->PSendSysMessage("{} is now VIP.", player->GetName());
    else
      handler->PSendSysMessage("{} already is VIP.", player->GetName());
    return true;
  }

  static bool HandleDel(ChatHandler* handler, Optional<PlayerIdentifier> player)
  {
    player = Resolve(handler, player);
    if (!player)
      return false;

    if (Set(player->GetGUID(), false))
      handler->PSendSysMessage("{} is no longer VIP.", player->GetName());
    else
      handler->PSendSysMessage("{} is not VIP.", player->GetName());
    return true;
  }

  static bool HandleGet(ChatHandler* handler, Optional<PlayerIdentifier> player)
  {
    player = Resolve(handler, player);
    if (!player)
      return false;

    handler->PSendSysMessage("{} is {}VIP.", player->GetName(), IsVip(player->GetGUID()) ? "" : "not ");
    return true;
  }
};
} // namespace

void AddVipScripts()
{
  new VipWorldScript();
  new VipCommandScript();
}
} // namespace arenacraft::vip
