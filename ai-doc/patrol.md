# Patrol

Players review reported arena replays and decide on bans, without knowing who they are judging.
Code: `src/game/Arenacraft/patrol/` (`PatrolRules` = pure rules + tests, `Patrol` = service, `PatrolScripts` =
commands). Tables: `patrol_case`, `patrol_report`, `patrol_verdict` (characters db,
`data/sql/updates/db_characters/2026_11_02_02.sql`). Needs replays (`AC_REPLAY_ENABLED`, see `replay.md`).

## Flow

1. **Report** - `.replay report <replay id> <name> <category>`. Only a participant of that match may report, only
   another participant. One case per (replay, suspect); more reports add categories to it. A closed case can't be
   reported again. The suspect's solo queue rating is stored on the case (`suspect_rating`).
2. **Accept** - `.patrol accept` picks a random open case the account has not judged, did not report and did not
   play in, whose suspect rating is inside the patroller's rating window. Needs the minimum solo queue games and the
   usual replay-watching conditions. The case's replay is played; the replay id is never shown.
3. **Review** - every participant is renamed: the suspect is `Suspect`, the others `Player1`..`Player5`. The camera
   follows the suspect (`.patrol camera` frees it). Seek/pause work as in any replay (`replay.md`); `.replay leave`
   ends the replay early.
4. **Verdict** - when the replay ends the patroller answers one yes/no question per enabled category
   (`.patrol yes` / `.patrol no`). `.patrol skip` passes on the case instead (recorded, never offered again, not
   counted).
5. **Decision** - after `Verdicts` verdicts the case closes. Categories with `YesToBan`+ "yes" convict; the
   punishment is applied (`BanMgr`, author `Patrol`, reason `Patrol: <categories>`), or with `Punishment = none` the
   case is only marked guilty (status 3) for staff. Otherwise the case is cleared.

Categories (`PatrolRules.hpp`): cheating (speed/teleport/fly/wall-walk hacks), automation (autokick, autofake,
auto-interrupt, bots, rotation scripts), griefing (throwing, AFK, feeding), win trading. Aliases such as `bot`,
`autokick`, `grief`, `wintrade` are accepted by the report command.

Staff: `.patrol cases` shows the active settings, open cases (with suspect rating, verdict and report counts) and
cases found guilty but not punished.

## Configuration

Env-only like the rest (`.env.example`), read at startup and on `.reload config` (`PatrolConfig.cpp`).

| Env | Default | Meaning |
| --- | --- | --- |
| `AC_PATROL_ENABLED` | `0` | master switch for reports and new reviews (running reviews can still be answered) |
| `AC_PATROL_MIN_GAMES` | `50` | solo queue season games needed to patrol |
| `AC_PATROL_RATING_WINDOW` | `0` | patroller's soloq rating within +-N of the suspect's rating at report time; 0 = any |
| `AC_PATROL_STAFF_EXEMPT` | `1` | staff skip the games / rating requirements |
| `AC_PATROL_CATEGORIES` | `all` | reportable categories = questions asked: `cheating,automation,griefing,wintrading` |
| `AC_PATROL_MAX_REPORTS_PER_DAY` | `10` | per reporting account; 0 = unlimited |
| `AC_PATROL_REPORT_WINDOW_HOURS` | `0` | max match age for a report; 0 = while the replay exists |
| `AC_PATROL_VERDICTS` | `5` | verdicts that close a case |
| `AC_PATROL_YES_TO_BAN` | `4` | yes votes in one category that convict (min 1) |
| `AC_PATROL_PUNISHMENT` | `account` | `account` / `character` ban, or `none` (mark only) |
| `AC_PATROL_BAN_DURATION` | `7d` | BanMgr duration (`30m`, `12h`, `1d12h`, ...); `0` = permanent |

Settings apply to what happens next: an open case closes by the `Verdicts` / `YesToBan` in force when its last
verdict arrives; the rating window uses the rating stored when the case was opened. Suspects without a soloq team
have rating 0, so with a window they only reach patrollers rated up to the window.

## Anonymity

- `WorldSession::SendNameQueryOpcode` answers with the alias for a reviewer (`patrol::AliasFor`); on replay start the
  aliases are also pushed unsolicited so names the client already cached are overwritten, and the real names are
  pushed back when the review ends.
- Guild names are not answered while reviewing (`HandleGuildQueryOpcode`). Pet names are unknown anyway (the recorded
  pets do not exist on the server).
- `.replay list` and `.replay <id>` are refused while reviewing.
- Not hidden: class, race and appearance; GUIDs (an addon that already knows a character's GUID could tell who it
  is - hiding that needs GUID rewriting in the recorded packets).

## Limits

- The replay is what the recording participant's client saw (see `replay.md`): a suspect stealthed to that client is
  not visible.
- Reviews live in memory: a logout drops an unfinished review (no verdict). A replay that fails to start keeps the
  review; `.patrol accept` retries it.
- Retention (`AC_REPLAY_KEEP_DAYS` / `MAX_TOTAL` / `MAX_PER_PLAYER`) skips recordings with an open case.
- Eligibility uses the soloq team's season games and rating, so it resets with the season.
