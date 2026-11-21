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
- Delete: `Player::LeaveAllArenaTeams` disbands solo teams (frees the name).
- Standard 2v2 charters cannot be bought or turned in.

## Player actions (`ArenaTeamHandler`)
- Leave / Disband on a solo team = reset (rating 1400, games/wins 0, MMR kept). Blocked while invited to
  a match / fighting. Invite / Remove / Leader are ignored.
- Captain checks for character delete and faction change ignore solo teams
  (`ArenaTeamMgr::GetNonSoloArenaTeamByCaptain`).

## Queue (`HandleBattlemasterJoinArena`, rated 2v2)
- Party of exactly two; each member's own team is used (`Group::CanJoinBattlegroundQueue` skips the
  same-team-id checks for slot 0).
- Party MMR gap must be <= 300 (`MaxPartyMmrGap`). Queue rating/MMR = average of both; `ateamId` = leader's team.
- Leave-after-invite penalty and arena logs use the player's own team.

## Match end (`Arena::ResolveSolo2v2`)
Each present player is rated on their own team against the opposing party's average MMR using the stock
formulas; scoreboard shows the average change per side. Leavers lose on their own team. Arena logs keep the leaders' team ids.
