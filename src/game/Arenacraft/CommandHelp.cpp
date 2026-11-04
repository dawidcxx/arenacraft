#include "CommandHelp.hpp"

#include "AccountMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Common.h"
#include "StringFormat.h"
#include "WorldSession.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace arenacraft
{
namespace
{
// One-line descriptions, keyed by the full command path. Commands not listed fall back to the first useful line of
// their `command` table help text.
std::map<std::string, std::string, std::less<>> const Descriptions = {
    // player
    {"help", "this list; .help <command> explains one command"},
    {"commands", "every command name you can use, without descriptions"},
    {"soloq join", "join the solo queue from anywhere"},
    {"challenge", ".challenge <name>: unrated arena match against a player (with your parties, up to 2v2)"},
    {"replay", ".replay <id>: watch a recorded arena match (not one you played in)"},
    {"replay watch", ".replay watch <id>: same as .replay <id>"},
    {"replay list", ".replay list [name]: newest recordings, or only those of a character"},
    {"replay leave", "stop watching a replay"},
    {"replay view", ".replay view [name]: watch the replay through a player's eyes; no name = your own camera"},
    {"spect", "arena spectator commands"},
    {"spect spectate", ".spect spectate <name>: watch a live arena a player is in"},
    {"spect watch", ".spect watch <name>: follow another player of the arena you spectate"},
    {"spect leave", "stop spectating"},
    {"gear stats", "item level and key stats of you or the selected player"},
    {"gm ingame", "game masters currently online"},
    {"server info", "server version, uptime and players online"},
    {"server motd", "message of the day"},
    {"save", "save your character"},
    {"dismount", "dismount"},
    {"account", "your account security level and expansion"},
    {"account password", ".account password <old> <new> <new>: change your password"},
    {"account 2fa setup", "set up two-factor authentication"},
    {"account 2fa remove", "remove two-factor authentication"},
    {"account lock country", "lock your account to your current country"},
    {"account lock ip", "lock your account to your current IP"},
    // staff
    {"gm help", ".gm help [group]: this overview, or every command of a group"},
    {"soloq status", "queue size, role breakdown and whether a match can form"},
    {"soloq list", "every queued player with role, class, spec, rating, MMR and wait"},
    {"soloq pending", "soloq arenas waiting for a result"},
    {"soloq clear", "remove everyone from the solo queue"},
    {"soloq debug", "toggle bypassing the character readiness checks"},
    {"soloq flex", "toggle flex composition (any two DPS + a healer)"},
    {"vip add", ".vip add [name]: flag the character (or selection) as VIP"},
    {"vip del", ".vip del [name]: remove the VIP flag"},
    {"vip get", ".vip get [name]: show whether the character is VIP"},
};

// Player-level commands that only exist for addons or are internal; they would just confuse in .help.
std::set<std::string, std::less<>> const HiddenPlayerCommands = {
    "account set 2fa",
    "debug movement",
    "spect version",
    "spect reset",
};

// Staff commands that are ours; .gm help lists them first, with descriptions.
std::vector<std::string_view> const ArenacraftStaffCommands = {
    "soloq status", "soloq list", "soloq pending", "soloq clear", "soloq debug",
    "soloq flex",   "vip add",    "vip del",       "vip get",
};

// First useful line of a `command` table help text: the description after "Syntax: ..." if there is one,
// otherwise the first line. Trimmed to keep a chat line short.
std::string Summarize(std::string_view help)
{
  std::vector<std::string> lines;
  std::string              current;
  for (char c : help)
  {
    if (c == '\r')
      continue;
    if (c == '\n')
    {
      lines.push_back(current);
      current.clear();
    }
    else
      current += c;
  }
  lines.push_back(current);

  std::string summary;
  for (std::string const& line : lines)
  {
    std::string_view trimmed = line;
    while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t'))
      trimmed.remove_prefix(1);
    if (trimmed.empty() || trimmed.starts_with("Syntax"))
      continue;
    summary = trimmed;
    break;
  }
  if (summary.empty() && !lines.empty())
    summary = lines.front();

  constexpr std::size_t MaxLength = 140;
  if (summary.size() > MaxLength)
    summary = summary.substr(0, MaxLength - 3) + "...";
  return summary;
}

std::string DescriptionOf(Acore::ChatCommands::CommandListing const& command)
{
  if (auto it = Descriptions.find(command.Name); it != Descriptions.end())
    return it->second;
  return Summarize(command.Help);
}

void SendEntry(ChatHandler* handler, Acore::ChatCommands::CommandListing const& command)
{
  std::string const description = DescriptionOf(command);
  if (description.empty())
    handler->PSendSysMessage("  |cff00ff00.{}|r", command.Name);
  else
    handler->PSendSysMessage("  |cff00ff00.{}|r - {}", command.Name, description);
}

std::string_view GroupOf(std::string_view name) { return name.substr(0, name.find(' ')); }
} // namespace

void ShowPlayerHelp(ChatHandler* handler)
{
  handler->SendSysMessage("Commands you can use (.help <command> for details):");
  for (Acore::ChatCommands::CommandListing const& command : Acore::ChatCommands::ListUsableCommands(*handler))
  {
    if (command.RequiredLevel != SEC_PLAYER || HiddenPlayerCommands.contains(command.Name))
      continue;
    SendEntry(handler, command);
  }

  if (handler->GetSession() && !AccountMgr::IsPlayerAccount(handler->GetSession()->GetSecurity()))
    handler->SendSysMessage("Staff: .gm help lists the commands of your security level.");
}

void ShowStaffHelp(ChatHandler* handler, std::string_view group)
{
  std::vector<Acore::ChatCommands::CommandListing> const usable = Acore::ChatCommands::ListUsableCommands(*handler);

  if (!group.empty())
  {
    bool found = false;
    for (Acore::ChatCommands::CommandListing const& command : usable)
    {
      if (GroupOf(command.Name) != group)
        continue;
      if (!found)
        handler->PSendSysMessage("Commands of .{} you can use (.help <command> for details):", group);
      found = true;
      SendEntry(handler, command);
    }
    if (!found)
      handler->PSendSysMessage("No command group .{} you can use. Type .gm help for the list of groups.", group);
    return;
  }

  handler->SendSysMessage("Arenacraft staff commands:");
  for (std::string_view name : ArenacraftStaffCommands)
    for (Acore::ChatCommands::CommandListing const& command : usable)
      if (command.Name == name)
        SendEntry(handler, command);

  // every other group that has at least one staff-level command, with its count
  std::map<std::string, uint32, std::less<>> groups;
  for (Acore::ChatCommands::CommandListing const& command : usable)
    if (command.RequiredLevel > SEC_PLAYER)
      ++groups[std::string(GroupOf(command.Name))];

  handler->SendSysMessage("Other command groups (.gm help <group> lists one, .help for player commands):");
  std::string line;
  for (auto const& [name, count] : groups)
  {
    std::string entry = Acore::StringFormat("{} ({})", name, count);
    if (!line.empty() && line.size() + entry.size() + 2 > 200)
    {
      handler->PSendSysMessage("  {}", line);
      line.clear();
    }
    if (!line.empty())
      line += ", ";
    line += entry;
  }
  if (!line.empty())
    handler->PSendSysMessage("  {}", line);
}
} // namespace arenacraft
