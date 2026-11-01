/*
 * Resolve a character's account alias (set by the Discord bot).
 *
 *   .alias get [character]   character -> account -> alias, or the account name
 *
 * With no argument the currently selected player is used. GM-only for now
 * because the fallback reveals the account (Discord) name.
 *
 * The output is one machine-readable line so future addons can parse it:
 *
 *   ALIAS:<character>=<alias-or-account>
 *
 * The right-hand side hides the account name whenever an alias is set.
 */

#include "AccountMgr.h"
#include "CharacterCache.h"
#include "Chat.h"
#include "CommandScript.h"
#include "DatabaseEnv.h"
#include "ObjectGuid.h"
#include "Player.h"

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>

using namespace Acore::ChatCommands;

namespace
{
// Positive results only: a miss stays uncached and is retried on the next call.
// The bot writes aliases from another process, so the TTL bounds staleness
// after a set/reset instead of relying on cross-process invalidation.
constexpr std::chrono::seconds AliasCacheTtl{60};

struct CachedAlias
{
  std::string                           alias;
  std::chrono::steady_clock::time_point expiresAt;
};

std::unordered_map<uint32, CachedAlias> _aliasCache;

std::optional<std::string> LookupAlias(uint32 accountId)
{
  auto const now = std::chrono::steady_clock::now();

  if (auto const it = _aliasCache.find(accountId); it != _aliasCache.end())
  {
    if (it->second.expiresAt > now)
      return it->second.alias;
    _aliasCache.erase(it);
  }

  QueryResult result = LoginDatabase.Query("SELECT alias FROM account_alias WHERE account_id = {}", accountId);
  if (!result)
    return std::nullopt;

  std::string alias = result->Fetch()[0].Get<std::string>();
  if (alias.empty())
    return std::nullopt;

  _aliasCache[accountId] = CachedAlias{alias, now + AliasCacheTtl};
  return alias;
}
} // namespace

class alias_commandscript : public CommandScript
{
public:
  alias_commandscript() : CommandScript("alias_commandscript") {}

  ChatCommandTable GetCommands() const override
  {
    static ChatCommandTable aliasTable = {
        {"get", HandleAliasGetCommand, SEC_GAMEMASTER, Console::Yes},
    };
    static ChatCommandTable commandTable = {{"alias", aliasTable}};
    return commandTable;
  }

  static bool HandleAliasGetCommand(ChatHandler* handler, Optional<std::string> characterArg)
  {
    std::string characterName;
    if (characterArg)
      characterName = *characterArg;
    else if (Player* target = handler->getSelectedPlayer())
      characterName = target->GetName();
    else
    {
      handler->SendSysMessage("No target selected. Usage: .alias get <character>");
      handler->SetSentErrorMessage(true);
      return false;
    }

    ObjectGuid const guid = sCharacterCache->GetCharacterGuidByName(characterName);
    if (guid.IsEmpty())
    {
      handler->PSendSysMessage("No character named \"{}\".", characterName);
      handler->SetSentErrorMessage(true);
      return false;
    }

    uint32 const accountId = sCharacterCache->GetCharacterAccountIdByGuid(guid);
    if (!accountId)
    {
      handler->PSendSysMessage("No account found for character \"{}\".", characterName);
      handler->SetSentErrorMessage(true);
      return false;
    }

    std::string canonicalName;
    if (!sCharacterCache->GetCharacterNameByGuid(guid, canonicalName))
      canonicalName = characterName;

    if (std::optional<std::string> alias = LookupAlias(accountId))
    {
      handler->PSendSysMessage("ALIAS:{}={}", canonicalName, *alias);
      return true;
    }

    std::string accountName;
    if (!AccountMgr::GetName(accountId, accountName))
      accountName = "unknown";

    handler->PSendSysMessage("ALIAS:{}={}", canonicalName, accountName);
    return true;
  }
};

void AddSC_alias_commandscript() { new alias_commandscript(); }
