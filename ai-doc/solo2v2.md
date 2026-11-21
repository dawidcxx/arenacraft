# Solo 2v2 and personal solo teams

Rated 2v2 has no shared teams. Every character owns a single-member 2v2 `ArenaTeam`
(`<Name>'s 2v2`, 1400 rating / 1500 MMR). The SoloQ 5v5 team (`<Name>'s SoloQ`, see `soloq.md`) is the
same idea. Both are "solo teams": `ArenaTeam::IsSoloTeam()`.

Code: `src/game/Arenacraft/solo2v2/` (`Solo2v2Rules.hpp` pure rules + tests, `Solo2v2Team.*` team management).

## Lifecycle
- Created by `solo2v2::EnsureTeam` at character creation (`CharacterHandler` create callback) and on every login
  (backfills missing teams). Migration `2026_11_21_00.sql` pre-creates one for every existing character and
  **drops all stock 2v2 teams** (MMR for slot 0 reset to 1500).
- Rename: `CharacterCache::UpdateCharacterData` calls `solo2v2::OnCharacterRenamed` when the name changed
  (2v2 and SoloQ team are renamed; `ArenaTeam::Rename` skips charter-name validation).
- Delete: `Player::LeaveAllArenaTeams(guid, true)` disbands personal solo teams (captain is the only member, frees
  the name). Leaving for any other reason (`LeaveAllArenaTeams(guid, false)`, i.e. faction change) leaves them
  untouched - they are personal, so there is no other member to split off and nothing to reset.
- Standard 2v2 and 5v5 charters cannot be bought or turned in. 5v5 is not a stock bracket: the 5v5 slot belongs
  to the SoloQ team (`IsSoloTeam()` is effectively "personal or SoloQ"), and only the SoloQ NPC creates it.

## Player actions (`ArenaTeamHandler`)
- Leave / Disband on a solo team = reset (rating 1400, games/wins 0, MMR kept). Blocked while invited to
  a match / fighting. Invite / Remove / Leader are ignored.
- Captain checks for character delete and faction change ignore personal solo teams
  (`ArenaTeamMgr::GetNonSoloArenaTeamByCaptain`, i.e. `ArenaTeam::IsPersonalTeam()`: solo type + sole member), so a
  SoloQ/2v2 leader can delete the character or change faction; the faction change keeps the team as-is (no rating
  reset, no rename beyond the character rename). A legacy stock 5v5 team (charters can no longer be created) is not
  personal once it has other members, so it keeps the stock rules.

## Queue (`HandleBattlemasterJoinArena`, rated 2v2)
- Party of exactly two; each member's own team is used (`Group::CanJoinBattlegroundQueue` skips the
  same-team-id checks for slot 0).
- Party MMR gap must be <= 300 (`MaxPartyMmrGap`). Queue rating/MMR = average of both; `ateamId` = leader's team.
- Leave-after-invite penalty and arena logs use the player's own team.

## Match end (`Arena::ResolveSolo2v2`)
Each present player is rated on their own team against the opposing party's average MMR using the stock
formulas; scoreboard shows the average change per side. Leavers lose on their own team. Arena logs keep the leaders' team ids.
