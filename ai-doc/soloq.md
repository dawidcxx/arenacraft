# Solo Queue (soloq)

Living notes for the arena solo-queue subsystem. Code lives in
`src/game/Arenacraft/soloq/` under `arenacraft::soloq`.

Status: the isolated queue/matchmaker, the post-match rating adjustment and the
arena pop are implemented. A temporary NPC front-end exists for end-to-end
testing; players get the client "in queue" badge and, when a match forms, the
rated 5v5 arena invite ("Enter Battle") dialog. The queue/badge track is 5v5
(an otherwise unused core track, so real 3v3 arena teams keep working), but the
arena instance spawned for the match is created as **3v3**, so inside the game
the match is played, scored and ready-checked as 3v3 (6/6). The player's
persistent team is a 5v5 `ArenaTeam`.

To get the badge, join/leave registers the player in the real 5v5 battleground
queue. `SoloqBattlegroundScript` returns `false` from `OnQueueUpdateValidity` for
rated 5v5, so the core's own matchmaking never creates an arena from those
entries (this globally disables core rated 5v5 matchmaking - intended, as soloq
occupies that track). When soloq finds a match it creates the arena itself and
invites the six players (see `CreateArenaForMatch`).

## Files

| File | Purpose |
| --- | --- |
| `Types.hpp` | `Role`, `QueuedPlayer`, `Team`, `Match`, `playersOf`, tuning constants |
| `Roles.hpp/.cpp` | `roleFor(Classes, specIndex) -> optional<Role>` |
| `SoloqQueue.hpp/.cpp` | the queue container + exhaustive best-fit matchmaker |
| `Outcome.hpp/.cpp` | `resolveMatch(Match, MatchResult)` - post-match Elo adjustment |
| `SoloqService.hpp/.cpp` | singleton: queue + pending arenas + `tick`/`registerMatch`/`takeMatch` |
| `SoloqTeam.hpp/.cpp` | the player's real 5v5 `ArenaTeam` (visible in the PvP pane, persisted in the characters DB) |
| `SoloqArenaQueue.hpp/.cpp` | registers/removes players in the real 5v5 battleground queue (client badge) and creates/invites a 3v3 arena for a match |
| `SoloqNpc.hpp/.cpp` | `SoloqNpc` (gossip menu on entry 20810), `SoloqDriver` (world tick) and `SoloqBattlegroundScript` (suppresses core 5v5 matchmaking) |
| `SoloqEvents.hpp/.cpp` | builds and publishes the `soloq-matchup` Redis event when a match pops |
| `CharacterCheck.hpp/.cpp` | pure character readiness check (talents, gear, enchants, gems, glyphs) + `Player` snapshot |
| `TalentRanks.hpp` | generated, curated `TalentRankRules` (rank 1 -> max rank) enforced by the readiness check |
| `*_test.cpp` | doctest unit tests (co-located, auto-discovered by the game test target) |

The pure logic (`Types`/`Roles`/`SoloqQueue`/`Outcome`) only depends on the core
`Classes` enum from `SharedDefines.h`. It never touches `Player`, which keeps the
tests fast and isolated. The core-facing pieces (`SoloqService` NPC glue,
`SoloqNpc`, `SoloqDriver`) are the only ones compiled against the game.

## Data model

```cpp
using PlayerId = uint64_t;                 // raw ObjectGuid
enum class Role : uint8_t { Melee, Caster, Healer };

struct QueuedPlayer { PlayerId id; Classes classId; uint8_t specIndex; uint32_t rating; uint32_t mmr; TeamId teamId; };
struct Team  { QueuedPlayer melee, caster, healer; };   // named slots encode composition
struct Match { Team a, b; };
```

`specIndex` is the tab page the player had the most talent points in at queue
time (`Player::GetMostPointsTalentTree()`). `rating` starts at 1400 and `mmr` at
1500 (constants in `namespace tuning`). `teamId` is the player's race faction,
carried only as informational metadata for the Redis event; it is not a
matchmaking input.

## Role table (`roleFor`)

| Class | spec 0 | spec 1 | spec 2 |
| --- | --- | --- | --- |
| Warrior | Melee | Melee | Melee |
| Paladin | Healer | Melee | Melee |
| Hunter | Melee | Melee | Melee |
| Rogue | Melee | Melee | Melee |
| Priest | Healer | Healer | Caster |
| Death Knight | Melee | Melee | Melee |
| Shaman | Caster | Melee | Healer |
| Mage | Caster | Caster | Caster |
| Warlock | Caster | Caster | Caster |
| Druid | Caster | Melee | Healer |

All hunter specs count as Melee. Tank specs are allowed and treated as Melee.
Unknown class / spec outside 0..2 returns `nullopt` and is rejected by the queue.

## Matchmaking (`SoloqQueue`)

A team is `{melee, healer, caster}`; a match is two teams (2 of each role). Needs
at least 2 of every role.

- Time is injected via `update(std::chrono::milliseconds elapsed)` - never read
  from the wall clock - so tests are deterministic.
- Each queued player accumulates wait time. Their acceptable MMR window is
  `min(MaxWindow, InitialWindow + MmrStep * floor(waited / StepInterval))`,
  symmetric around their MMR (`InitialWindow = 150`, `MmrStep = 50`,
  `StepInterval = 30s`, `MaxWindow = 500`). The nonzero base means a fresh
  player already searches ±150 instead of requiring an exact MMR match, so a
  slightly diverged pool still pops instead of stalling.
- A candidate six-set is **valid** only if every pair satisfies
  `gap <= max(window_i, window_j)` (the more-patient player's window governs).
- **No faction input**: the queue is a single shared pool. Teams are formed from
  role, class-distinctness and MMR only; a team may mix races freely.
- **No class stacking per team**: a partition where team A or team B would field
  the same class twice is rejected (e.g. Shadow Priest caster + Discipline
  Priest healer). If no partition of the six avoids it, the candidate is skipped
  entirely. `makeCandidate` returns `nullopt` in that case.
- Exhaustive best-fit: enumerate all `(2 melee, 2 caster, 2 healer)` valid sets,
  try the 4 ways to split them into two teams (keeping only class-distinct
  teams), and score by `team imbalance -> MMR spread -> longest total wait ->
  insertion order`. Emit the best match, remove those six, repeat until nothing
  valid remains.
- `update` returns the drained `std::vector<Match>`; matched players are removed.

## Post-match adjustment (`resolveMatch`)

Pure function: given a `Match` and `MatchResult::{TeamAWin,TeamBWin}` it returns
six `RatingUpdate{id, mmr, rating, delta}` (team A first, then team B).

- Elo expected score with `EloScale = 400`, `KFactor = 16`.
- The winner gains and the loser loses the same amount (zero-sum); an underdog
  win is worth more than a favourite win.
- `mmr`/`rating` both move by `delta` and clamp at 0.

`CreateArenaForMatch` registers the arena instance id -> `Match` in
`SoloqService`. `SoloqBattlegroundScript::OnBattlegroundEnd` looks it up, maps
the winning `TeamId` to `MatchResult`, calls `resolveMatch`, then applies each
result to the player's 5v5 `ArenaTeam` via `ApplySoloqResult` and sys-messages
them. `OnBattlegroundDestroy` takes the still-pending entry if
the arena never finished (all invites declined) and emits a non-finished
`soloq-matchup-ended` event.

`ApplySoloqResult` (`SoloqTeam.hpp/.cpp`) is what keeps the "fake" 5v5 soloq
team in sync with the actual results. It writes the Elo rating/MMR from
`resolveMatch` (not the core's own `ArenaTeam::WonAgainst` formula), bumps the
team's `WeekGames`/`SeasonGames`/wins and the member's equivalent counters,
updates `MaxMMR`, recomputes `Rank` like `ArenaTeam::FinishGame`, and refreshes
the player's `PLAYER_FIELD_ARENA_TEAM_INFO_1_1` fields
(`ARENA_TEAM_GAMES_WEEK`, `ARENA_TEAM_GAMES_SEASON`, `ARENA_TEAM_WINS_SEASON`,
`ARENA_TEAM_PERSONAL_RATING`) so the client PvP pane shows the new personal
rating and record without a relog. Finally it persists via `SaveToDB(true)` and
`NotifyStatsChanged()`.

Because the queue/badge track is 5v5 but the arena instance is 3v3, the core's
`Battleground::RemovePlayerAtLeave` cleanup removes `BATTLEGROUND_QUEUE_3v3`,
leaving the player's 5v5 queue id set. That keeps the client badge and blocks
requeueing. `SoloqBattlegroundScript` clears the 5v5 id itself:
`OnBattlegroundEnd` for every rated participant and
`OnBattlegroundRemovePlayerAtLeave` for anyone leaving a started match early.

Clearing the 5v5 queue id must also drop the real `BattlegroundQueue` entry.
A player who was invited but never accepted (e.g. the arena ended as a walkover
when one team never ported) is still in `m_QueuedPlayers`; the core's scheduled
`BGQueueRemoveEvent` keys off the queue id, so clearing the id alone would strand
the entry forever and the next join would hit the `AddGroup`
duplicate-player assertion (crash). `ClearSoloqQueueId(player, bgInstanceId)`
(`SoloqNpc.cpp`) removes the entry via `GetPlayerGroupInfoData`/`RemovePlayer`
before clearing the id, but only when the entry is actually for that arena
(`ginfo.IsInvitedToBGInstanceGUID == bgInstanceId`). This matters because a
player who forfeits mid-match can requeue immediately: when the old match
finally ends, its cleanup must not yank them out of the new queue.

A player can forfeit an arena mid-match, including while in combat
(`HandleBattlefieldLeaveOpcode` allows combat-leave for `bg->isArena()` only;
plain battlegrounds still block it). Both cleanup paths above run through
`BattlegroundMap::RemovePlayerFromMap` -> `Battleground::RemovePlayerAtLeave`, so
the soloq queue id is cleared and the match is resolved by `OnBattlegroundEnd`
when the arena ends. `Arena::RemovePlayerAtLeave` also strips the arena
dampening aura (29659), which is applied raw and would otherwise leak out of the
match.

The leaver's rating is still settled when the match ends (`OnBattlegroundEnd`):
the result is applied to their persistent 5v5 team exactly like the other five,
so if their side wins they gain rating. `ApplySoloqResult` returns false and
nothing is written (no sys message) when the player no longer has a soloq team
(e.g. they deleted it after leaving). Offline players are skipped entirely.

## Character readiness gate

Before a player is enqueued, `SoloqService::characterProblem(Player*)` runs a
one-off sanity check so unfinished characters are not queued by accident. It
rejects, in order:

- unspent talent points (`Player::GetFreeTalentPoints() > 0`),
- a talent learned at rank 1 without its max rank (`TalentRankRules`, see below),
- any empty required gear slot (`RequiredEquipmentSlots` in `CharacterCheck.hpp`:
  head, neck, shoulders, chest, waist, legs, feet, wrists, hands, both rings,
  both trinkets, back, mainhand),
- a required enchant missing on any of `RequiredEnchantedSlots` (head,
  shoulders, chest, bracers, hands, legs, boots); the item must carry a
  permanent enchant (`PERM_ENCHANTMENT_SLOT`),
- any equipped item with a built-in socket that has no gem,
- any *enabled major* glyph slot (`RequiredGlyphSlots` in `CharacterCheck.hpp`:
  the GlyphSlot.dbc major slots, 0-based indices 0, 3 and 5) without a glyph.
  Minor glyph slots are ignored: minor glyphs are cosmetic.

The max-rank rule covers abilities whose rank 1 comes from the talent tree while
the higher ranks are bought elsewhere (class trainer/vendor) - e.g. Penance
(`47540` -> `53007`). The list is `TalentRanks.hpp` (`TalentRankRules`), generated
by `scripts/src/gen_talent_rank_rules.ts` from `Talent.dbc` + `spell_ranks`. It
is deliberately conservative: only single-rank talents whose chain extends past
that rank are included, so a legitimate partial multi-rank build (3/5) is never
flagged, and a rule is only emitted when the max rank is actually sold by one of
the class trainers (`ClassTrainerEntry`), so every violation is fixable at the
vendor. It is a plain reviewable table; curate it by deleting rows. The rule is
checked only when the character knows the first rank and not the max rank; a
character with neither (or only the max) passes.

Cosmetic slots (shirt, tabard) and the optional offhand/ranged slots are not
required: two-handers and many casters legitimately leave them empty. The check
is deliberately static: it does not validate gem colour, enchant quality or
which glyph is socketed.

The check is potentially costly, so a character that passes is remembered in
`SoloqService::_validatedCharacters` for the process lifetime; later gear or
talent changes are ignored. This is an anti-footgun aid, not cheat prevention.
The pure logic lives in `checkCharacter(CharacterSnapshot const&)` and is unit
tested in `CharacterCheck_test.cpp`; `snapshotCharacter(Player*)` builds the
snapshot from the live player.

## Service and the soloq team

`SoloqService::instance()` is a single global (single writer, no locking) and
owns only the queue plus pending arenas:

- `join(id, classId, specIndex, teamId, rating, mmr)` - enqueues a snapshot.
- `leave(id)` - dequeues.
- `tick(elapsed)` - advances the queue, returns drained matches.
- `registerMatch(bgInstanceId, match)` / `takeMatch(...)`.
- `queueSize`, `inQueue`, `waitingPlayers`.
- `characterProblem(player)` - cached character readiness gate (see above);
  `_validatedCharacters` remembers the ids that passed.
- `roleCounts()` - total `RoleCounts` recomputed on every queue mutation
  (`join`/`leave`/matching `tick`), so the gossip handler can read it without
  walking the queue.

`formatQueueStats(counts)` renders the single shared-queue stats block shown as
the NPC gossip page text (pure; unit-tested in `SoloqService_test.cpp`).

The player's rating/MMR is **not** stored here - it lives in a real 5v5
`ArenaTeam` (`SoloqTeam.hpp`): `FindSoloqTeam`, `CreateSoloqTeam` (starting
1400/1500, named `<name>'s SoloQ`), `DeleteSoloqTeam` (disbands),
`GetSoloqTeamInfo` and `ApplySoloqResult` (records a finished match; see the
post-match section). Because it is an arena team, it shows in the client's PvP
pane and persists in the characters DB across restarts. `CreateSoloqTeam` and
`ApplySoloqResult` both refresh the player's `PLAYER_FIELD_ARENA_TEAM_INFO_1_1`
fields, since `ArenaTeam::AddMember` only sets the team id/type.

## Temporary NPC

`SoloqNpc` hijacks creature entry **20810** ("Mehrdad"), giving it
`UNIT_NPC_FLAG_GOSSIP` and this menu:

- **Join SoloQ** - requires a team; passes the character readiness gate; enqueues
  and reports the queue size.
- **Leave SoloQ** - dequeues.
- **Create SoloQ Team** - creates the 5v5 `ArenaTeam` at 1400 rating / 1500 MMR
  (shows in the PvP pane immediately).
- **Delete SoloQ Team** - disbands the team (resets rating/MMR).

Opening the menu also replaces the NPC's default gossip page text with the live
queue counts:

```
SoloQ Queue
-----------------------

Players in queue: 6

Melee (1) Caster (5) Healer (0)
```

There is a single queue for everyone. The text is built from
`SoloqService::roleCounts()` and registered in memory via
`ObjectMgr::AddOrUpdateGossipText` under the custom id `StatsTextId` (9100002;
no DB row). Because the client caches npc text by id, `WorldSession::SendNpcTextUpdate`
is called first to push the fresh text (`SMSG_NPC_TEXT_UPDATE`) before the
gossip menu references it. `AddOrUpdateGossipText` / `SendNpcTextUpdate` are
general core additions (nothing soloq-specific in them).

Joining also calls `EnterArenaQueue` (`SoloqArenaQueue`): it adds the player to
the real `BATTLEGROUND_QUEUE_5v5` queue and sends `SMSG_BATTLEFIELD_STATUS`
(`STATUS_WAIT_QUEUE`, rated 5v5), which is what makes the client show the eye
badge and re-request it on login/map change. Leaving (NPC, `Delete Team`, or the
client's own "Leave Queue") removes the entry and clears the badge.
The player's persistent `SoloqTeam` is a **5v5** `ArenaTeam` (so it lives in the
5v5 PvP tab); only the match instance itself is 3v3.

`SoloqDriver` (`WorldScript::OnUpdate`) ticks the service every world update and
logs any formed match to the `server` log. Each tick it also drops anyone who
left the 5v5 queue through the client or logged out.

On a match, `CreateArenaForMatch` (`SoloqArenaQueue`) creates a rated 3v3 arena
via `BattlegroundMgr::CreateNewBattleground`, then calls
`BattlegroundQueue::InviteGroupToBG` for each of the six solo queue entries (team
A = `TEAM_ALLIANCE`, team B = `TEAM_HORDE`) and `StartBattleground`. That sends
`STATUS_WAIT_JOIN` to each player, which is the "Enter Battle" dialog; accepting
ports them into the instance. If the match can no longer be turned into an arena
(a player vanished), the six are dropped and told to requeue.

The arena instance itself is rated but has **no ArenaTeam** (`ArenaTeamId = 0`)
- the six players' own soloq 5v5 teams are not the match participants. So
`Arena::EndBattleground` is guarded to skip the arena-team rating/log block when
there is no team, instead of dereferencing null. `SoloqBattlegroundScript`
suppresses the core's rated-5v5 matchmaking so the core never creates a second
arena for the same queue.

The matchmaker builds 3-player teams. The queue is 5v5, but the arena instance is
created with `ARENA_TYPE_3v3`, so the match itself (scoreboard, ready check 6/6)
is treated as 3v3 while the queue badge stays on the 5v5 track.

## Team reaction (truce)

Reaction between players is normally race-based, which does not work once a team
can mix races. The core is patched directly
(`src/game/Entities/Unit/Unit.cpp`):

- `Unit::GetReactionTo`: two player-controlled units in the same battleground are
  friendly when they share a battleground team and hostile otherwise, checked
  before the race fallback. It reverts automatically when the arena ends
  (`GetBattleground()` becomes null), so normal rules apply outside the match.
- `Unit::PatchValuesUpdate`: same-team players are sent the viewer's faction
  template plus the sanctuary byte, so the client shows green names and allows
  follow, heal and resurrect. The opposing battleground team keeps its real
  faction and stays attackable.

Cross-faction groups are always allowed server-wide and the client party fake is
unconditional, so the same behaviour applies to ordinary mixed parties.

`CreateArenaForMatch` fixes the two sides: team A is battleground `TEAM_ALLIANCE`
and team B is `TEAM_HORDE`.

## Redis matchup event

When a match pops (i.e. `CreateArenaForMatch` has created the arena and sent
the invites), the core publishes a `soloq-matchup` event so external consumers
(web, Discord, overlays) can react. Publishing is **best-effort**: `RedisConn`
(`src/common/Redis/`) never blocks the world, reconnects on demand (10s backoff)
and no-ops when Redis is disabled or unreachable. Config is `AC_REDIS_ENABLED`,
`AC_REDIS_HOST`, `AC_REDIS_PORT`, `AC_REDIS_PASSWORD`, `AC_REDIS_DB` (see
`.env.example`).

Payload (`event = "soloq.matchup"`, see `SoloqEvents.hpp`):

```json
{
  "event": "soloq.matchup",
  "instanceId": 1234,
  "arenaType": 3,
  "matchup": { "instanceId": 1234, "arenaType": 3, "mapId": 617, "bracketId": 2, "queueType": 5, "startedAtMs": 1700000000000 },
  "players": [
    { "guid": "0x0000000000001000", "characterName": "Alpha", "accountId": 1, "accountName": "acc-alpha",
      "classId": 1, "specIndex": 0, "role": "melee", "teamId": 0, "faction": "alliance", "rating": 1500, "mmr": 1550 }
  ],
  "teamA": { "teamId": 0, "faction": "alliance", "averageRating": 1400, "averageMmr": 1450, "players": [/* 3 */] },
  "teamB": { "teamId": 1, "faction": "horde", "averageRating": 1300, "averageMmr": 1350, "players": [/* 3 */] },
  "rating": {
    "teamA": { "averageRating": 1400, "averageMmr": 1450, "winDelta": 9, "lossDelta": -7 },
    "teamB": { "averageRating": 1300, "averageMmr": 1350, "winDelta": 7, "lossDelta": -9 }
  }
}
```

- `players` is the flat union (team A first, then team B); `teamA`/`teamB` inline
  the same objects. Each player carries the role slot, rating/MMR snapshot, the
  character name and the auth account id/name.
- `guid` is serialized as a `0x`-prefixed hex **string** (raw `ObjectGuid` is a
  uint64 and would lose precision as a JSON number).
- `winDelta`/`lossDelta` come from the same Elo used by `resolveMatch`; they are
  the hypothetical result for each side (`Outcome.hpp` exposes
  `teamAverageMmr`/`teamAverageRating`/`winningRatingDelta`).
- `SoloqEvents_test.cpp` covers the pure `buildMatchupPayload` serializer.

When the arena ends (or is destroyed without finishing) the core publishes
`soloq-matchup-ended` from `SoloqBattlegroundScript`:

```json
{
  "event": "soloq.matchup.ended",
  "instanceId": 1234,
  "finished": true,
  "players": [ /* same shape as above, so consumers can recover a lost room by account name */ ]
}
```

`finished` is `true` when a winner ended the arena, `false` when it was destroyed
(invites declined). Both events are emitted exactly once per popped matchup:
`OnBattlegroundEnd` takes the pending match (normal path) and
`OnBattlegroundDestroy` takes it only if it is still pending (abort path).

## Commands

`src/game/Scripts/Commands/cs_soloq.cpp` (registered in `cs_script_loader.cpp`).

Player command (SEC_PLAYER), so players can queue from anywhere in the world
instead of walking back to the NPC:

- `.soloq join` - creates the player's 5v5 soloq team if missing, then runs the
  shared `JoinSoloqQueue` (see `SoloqArenaQueue`), which does the same validation
  and enqueue as the NPC's "Join SoloQ" option (readiness gate, badge, messages).

Admin commands:

- `.soloq status` - total queued, per-role counts, pending arenas, and a verdict
  on whether a match can form.
- `.soloq list` - every queued player: id, class, spec, role, rating, MMR, wait
  time. Use this to confirm you have 2/2/2.
- `.soloq pending` - arenas waiting for a result (instance id + both teams).
- `.soloq clear` - dequeue everyone and clear their badges.
- `.soloq debug on|off` - runtime bypass of the character readiness gate
  (`SoloqService::setSkipCharacterChecks`): while on, `Join SoloQ` skips the
  gear/enchant/gem/talent/glyph checks entirely. `off` (default) enforces them.
  Does not clear the per-character pass cache.

## Wiring

- Scripts are registered in `AddArenacraftScripts()` (`ArenacraftScripts.cpp`).
- Unit tests run with `zig build run-game-test` (the `game` target).
- Nothing uses `npc_vendor`/SQL; the whole system is code-defined.

## Out of scope / next sprints

- Class-stacking rules across the whole match rather than per team, if desired.
- Block talent/spec changes while queued (cheat prevention).
- Queue-count queries and richer NPC/UI feedback.
- Thread safety once there is more than one writer.
