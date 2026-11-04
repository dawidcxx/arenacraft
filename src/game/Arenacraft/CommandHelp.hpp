#pragma once

#include <string_view>

class ChatHandler;

// Readable command references for chat:
//   .help             every command a player can use, with a one-line description
//   .gm help          staff overview: arenacraft staff commands + the other command groups
//   .gm help <group>  every command of a group the caller can use (e.g. ".gm help npc")
// Both are built from the live command tree, so new commands show up without touching this file; the
// descriptions below only replace or fill in the `command` table help text.
namespace arenacraft
{
void ShowPlayerHelp(ChatHandler* handler);
void ShowStaffHelp(ChatHandler* handler, std::string_view group);
} // namespace arenacraft
