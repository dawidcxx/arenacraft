#pragma once

#include "ObjectGuid.h"

#include <optional>
#include <string>
#include <string_view>

class ChatHandler;
class Player;

// Patrol: players review reported arena replays and decide on bans. See ai-doc/patrol.md.
//
//   .replay report <id> <name> <category>   a participant reports another participant of a recorded match
//   .patrol accept                           watch a random open case; names are hidden, the suspect is "Suspect"
//   .patrol yes|no                           answer the questions asked when the replay ends
//   .patrol skip                             pass on the case (it is not offered again)
//
// Everything is configurable via Patrol.* / AC_PATROL_* (PatrolConfig.hpp): on/off, who may review (games,
// rating window), categories, report limits, verdicts per case, yes votes to convict and the punishment.
namespace arenacraft::patrol
{
// --- commands --------------------------------------------------------------------------------------------
void Report(ChatHandler* handler, Player* reporter, uint32 replayId, std::string_view suspectName,
            std::string_view category);
void Accept(ChatHandler* handler, Player* patroller);
void Answer(ChatHandler* handler, Player* patroller, bool yes);
void Skip(ChatHandler* handler, Player* patroller);
void Status(ChatHandler* handler, Player* patroller);
void ToggleFollow(ChatHandler* handler, Player* patroller);
void ListOpenCases(ChatHandler* handler);

// --- hooks -----------------------------------------------------------------------------------------------
// Replay playback of `replayId` started / ended for `viewer` (world thread). `watched` is false when the
// playback never started (viewer did not arrive).
void OnReplayStarted(Player* viewer, uint32 replayId);
void OnReplayEnded(ObjectGuid viewer, uint32 replayId, bool watched);
// The viewer jumped on the replay timeline: the recorded objects were rebuilt, so the camera is re-attached.
void OnReplaySeeked(Player* viewer, uint32 replayId);
// Drops an unfinished review (no verdict is recorded).
void OnLogout(Player* player);

// Registers the .patrol commands and the logout hook (PatrolScripts.cpp).
void AddPatrolScripts();

// Name the viewer must see for `target` while reviewing, if any (name query responses).
std::optional<std::string> AliasFor(ObjectGuid viewer, ObjectGuid target);
// True while `viewer` has a review open (guild names are hidden then).
bool IsReviewing(ObjectGuid viewer);
} // namespace arenacraft::patrol
