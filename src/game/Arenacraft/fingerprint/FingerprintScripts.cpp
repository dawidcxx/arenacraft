/*
 * Fingerprint commands (game masters), see ai-doc/fingerprint.md:
 *
 *   .fingerprint touch <name> [reason]   store the player's current macros, bindings and addons (online or not)
 *   .fingerprint compare <name>          compare the player with every stored fingerprint, top 5 in percent
 *   .fingerprint list                    stored fingerprints
 *   .fingerprint delete <id>             drop a stored fingerprint
 */
#include "FingerprintScripts.hpp"

#include "Chat.h"
#include "CommandScript.h"
#include "DatabaseEnv.h"
#include "Fingerprint.hpp"
#include "Log.h"
#include "Player.h"
#include "WorldSession.h"

#include <algorithm>

using namespace Acore::ChatCommands;

namespace arenacraft::fingerprint
{
namespace
{
constexpr std::size_t TopMatches = 5;

std::string Escaped(std::string text)
{
  CharacterDatabase.EscapeString(text);
  return text;
}

// Account-wide and character data of one kind (bindings or macros), account first.
std::string LoadAccountData(uint32 accountId, uint32 guid, AccountDataType global, AccountDataType perCharacter)
{
  std::string text;
  if (QueryResult result = CharacterDatabase.Query("SELECT data FROM account_data WHERE accountId = {} AND type = {}",
                                                   accountId, uint8(global)))
    text += (*result)[0].Get<std::string>();
  if (QueryResult result = CharacterDatabase.Query(
          "SELECT data FROM character_account_data WHERE guid = {} AND type = {}", guid, uint8(perCharacter)))
    text += "\n" + (*result)[0].Get<std::string>();
  return text;
}

RawFingerprint LoadCurrent(uint32 accountId, uint32 guid)
{
  RawFingerprint raw;
  raw.macros   = LoadAccountData(accountId, guid, GLOBAL_MACROS_CACHE, PER_CHARACTER_MACROS_CACHE);
  raw.bindings = LoadAccountData(accountId, guid, GLOBAL_BINDINGS_CACHE, PER_CHARACTER_BINDINGS_CACHE);
  if (QueryResult result =
          CharacterDatabase.Query("SELECT addons FROM account_addon_list WHERE account_id = {}", accountId))
    raw.addons = (*result)[0].Get<std::string>();
  return raw;
}

std::string Percent(double value) { return value < 0 ? "-" : Acore::StringFormat("{:.0f}%", value); }

class FingerprintCommandScript : public CommandScript
{
public:
  FingerprintCommandScript() : CommandScript("arenacraft::fingerprint::FingerprintCommandScript") {}

  ChatCommandTable GetCommands() const override
  {
    static ChatCommandTable fingerprintTable = {
        {"touch", HandleTouch, SEC_GAMEMASTER, Console::Yes},
        {"compare", HandleCompare, SEC_GAMEMASTER, Console::Yes},
        {"list", HandleList, SEC_GAMEMASTER, Console::Yes},
        {"delete", HandleDelete, SEC_ADMINISTRATOR, Console::Yes},
    };
    static ChatCommandTable commandTable = {{"fingerprint", fingerprintTable}};
    return commandTable;
  }

  static uint32 AccountOf(PlayerIdentifier const& player)
  {
    if (Player* online = player.GetConnectedPlayer())
      return online->GetSession()->GetAccountId();
    return sCharacterCache->GetCharacterAccountIdByGuid(player.GetGUID());
  }

  static bool HandleTouch(ChatHandler* handler, PlayerIdentifier player, Optional<Tail> reason)
  {
    uint32 const   accountId = AccountOf(player);
    uint32 const   guid      = player.GetGUID().GetCounter();
    RawFingerprint raw       = LoadCurrent(accountId, guid);
    Features const features  = extract(raw);

    if (features.macroLines.empty() && features.bindings.empty() && features.addons.empty())
    {
      handler->PSendSysMessage("{} has no synced macros, bindings or addons yet (the client syncs them while playing).",
                               player.GetName());
      return true;
    }

    std::string const by = handler->GetSession() ? handler->GetSession()->GetPlayerName() : "console";
    CharacterDatabase.DirectExecute(
        "INSERT INTO fingerprint (account_id, guid, name, reason, created_by, macros, bindings, addons) "
        "VALUES ({}, {}, '{}', '{}', '{}', '{}', '{}', '{}')",
        accountId, guid, Escaped(player.GetName()), Escaped(reason ? std::string(*reason) : std::string()), Escaped(by),
        Escaped(raw.macros), Escaped(raw.bindings), Escaped(raw.addons));

    handler->PSendSysMessage("Fingerprint of {} stored: {} macro lines, {} bindings, {} addons.", player.GetName(),
                             features.macroLines.size(), features.bindings.size(), features.addons.size());
    LOG_INFO("server", "fingerprint: {} stored the fingerprint of {} (account {})", by, player.GetName(), accountId);
    return true;
  }

  struct Stored
  {
    uint32      id;
    uint32      accountId;
    std::string name, reason, by, date;
    Features    features;
  };

  static bool HandleCompare(ChatHandler* handler, PlayerIdentifier player)
  {
    uint32 const   accountId = AccountOf(player);
    Features const current   = extract(LoadCurrent(accountId, player.GetGUID().GetCounter()));
    if (current.macroLines.empty() && current.bindings.empty() && current.addons.empty())
    {
      handler->PSendSysMessage("{} has no synced macros, bindings or addons to compare.", player.GetName());
      return true;
    }

    std::vector<Stored> stored;
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT id, account_id, name, reason, created_by, DATE_FORMAT(created_at, '%Y-%m-%d'), macros, bindings, "
            "addons FROM fingerprint"))
      do
      {
        Field* f = result->Fetch();
        stored.push_back({f[0].Get<uint32>(), f[1].Get<uint32>(), f[2].Get<std::string>(), f[3].Get<std::string>(),
                          f[4].Get<std::string>(), f[5].Get<std::string>(),
                          extract({f[6].Get<std::string>(), f[7].Get<std::string>(), f[8].Get<std::string>()})});
      } while (result->NextRow());

    // Rarity weights over every fingerprint plus the player compared.
    std::vector<Features> corpus;
    corpus.reserve(stored.size() + 1);
    for (Stored const& entry : stored)
      corpus.push_back(entry.features);
    corpus.push_back(current);
    Weights const weights(corpus);

    std::vector<std::pair<Similarity, Stored const*>> scored;
    for (Stored const& entry : stored)
      if (entry.accountId != accountId) // the player's own snapshots would always match
        scored.emplace_back(compare(current, entry.features, weights), &entry);
    if (scored.empty())
    {
      handler->SendSysMessage("No stored fingerprints of other accounts to compare with.");
      return true;
    }

    std::sort(scored.begin(), scored.end(),
              [](auto const& l, auto const& r) { return l.first.overall > r.first.overall; });
    if (scored.size() > TopMatches)
      scored.resize(TopMatches);

    handler->PSendSysMessage(
        "{} (account {}) - closest stored fingerprints (macros / addons / bindings):", player.GetName(), accountId);
    for (std::size_t i = 0; i < scored.size(); ++i)
    {
      auto const& [similarity, entry] = scored[i];
      handler->PSendSysMessage("{}. {} - #{} {} (account {}), {} by {}{} | {} / {} / {}", i + 1,
                               Percent(similarity.overall), entry->id, entry->name, entry->accountId, entry->date,
                               entry->by, entry->reason.empty() ? "" : ": " + entry->reason, Percent(similarity.macros),
                               Percent(similarity.addons), Percent(similarity.bindings));
    }
    return true;
  }

  static bool HandleList(ChatHandler* handler)
  {
    QueryResult result = CharacterDatabase.Query("SELECT id, name, account_id, DATE_FORMAT(created_at, '%Y-%m-%d'), "
                                                 "created_by, reason FROM fingerprint ORDER BY id DESC LIMIT 50");
    if (!result)
    {
      handler->SendSysMessage("No stored fingerprints.");
      return true;
    }
    handler->SendSysMessage("Stored fingerprints (newest first):");
    do
    {
      Field* f = result->Fetch();
      handler->PSendSysMessage("#{} {} (account {}), {} by {}{}", f[0].Get<uint32>(), f[1].Get<std::string>(),
                               f[2].Get<uint32>(), f[3].Get<std::string>(), f[4].Get<std::string>(),
                               f[5].Get<std::string>().empty() ? "" : ": " + f[5].Get<std::string>());
    } while (result->NextRow());
    return true;
  }

  static bool HandleDelete(ChatHandler* handler, uint32 id)
  {
    if (!CharacterDatabase.Query("SELECT 1 FROM fingerprint WHERE id = {}", id))
    {
      handler->PSendSysMessage("No fingerprint #{}.", id);
      return true;
    }
    CharacterDatabase.DirectExecute("DELETE FROM fingerprint WHERE id = {}", id);
    handler->PSendSysMessage("Fingerprint #{} deleted.", id);
    return true;
  }
};
} // namespace

void RememberAddons(uint32 accountId, std::vector<std::string> const& enabledAddons)
{
  if (!accountId)
    return;
  std::string list;
  for (std::string const& name : enabledAddons)
    list += name + "\n";
  CharacterDatabase.Execute("REPLACE INTO account_addon_list (account_id, addons, updated_at) VALUES ({}, '{}', NOW())",
                            accountId, Escaped(list));
}

void AddFingerprintScripts() { new FingerprintCommandScript(); }
} // namespace arenacraft::fingerprint
