# Arena replays

Code: `src/game/Arenacraft/replay/`. Tables: `arena_replay`, `arena_replay_player`
(characters db, `data/sql/updates/db_characters/2026_10_31_00.sql`).

## Config (env)

| env                        | default          | meaning                                                   |
|----------------------------|------------------|-----------------------------------------------------------|
| `AC_REPLAY_ENABLED`        | `0`              | master switch: record matches and allow playback          |
| `AC_REPLAY_BRACKETS`       | `2v2,3v3,soloq`  | `2v2`, `3v3`, `5v5`, `soloq`, `skirmish`, `all`           |
| `AC_REPLAY_KEEP_DAYS`      | `0`              | delete recordings older than N days                       |
| `AC_REPLAY_MAX_TOTAL`      | `0`              | keep the newest N recordings overall                      |
| `AC_REPLAY_MAX_PER_PLAYER` | `0`              | keep the newest N recordings per character                |

`0` = no limit. Limits are enforced right after each saved match (same
transaction as the insert); `KEEP_DAYS` also on worldserver startup. Recordings
with an open patrol case are never deleted by these limits. Per-player:
a match is deleted for all participants once it falls out of any one
participant's newest N.

## Commands (any player)

- `.replay <id>` / `.replay watch <id>` - watch
- `.replay list [name]` - newest 10, optionally only matches of `name`
- `.replay leave` - stop watching (`.spect leave` works too)
- `.replay pause` / `.replay resume` - pause / resume the replay you watch
- `.replay seek <target>` - jump on the timeline: `95` / `95s` (absolute seconds), `1:35` (m:ss), `+10` / `-10`
  (relative seconds); clamped to the recording
- `.replay state` - send the addon `STATE` message right away (for addons that just loaded)
- `.replay report <id> <name> <category>` - report a player of a match you played in to the patrol
  (see `patrol.md`)

## How it works

Recording (`ReplayRecorder`):
- starts when the gates open (`Battleground::_ProcessJoin`), before visibility
  widens, for arenas whose bracket is enabled. Soloq arenas are tagged with
  `Battleground::SetSoloq` in `SoloqArenaQueue`.
- the match is captured through one participant, the *primary* (first player of
  the first team): a snapshot of every object its client knows (+ the primary
  itself built for an outsider, + visible auras), then every observer-relevant
  packet sent to it (`isObserverOpcode`: object updates, others' movement,
  spells, auras, combat logs, world states). Self-only packets (cooldowns,
  forced speed changes, chat, ...) are dropped.
- the server never echoes a player's own movement to them, so the primary's
  movement comes from a *secondary* participant (first player of the other
  team), filtered to the primary's guid.
- capture hook: `WorldSession::SendPacket` -> `SetReplayTap`.
- ends in `Battleground::EndBattleground`; matches under 10 s are dropped. The
  packet list is zlib-compressed (`ReplayFormat`) and stored as a blob.

Playback (`Playback`):
- creates a private, unrated, never-started copy of the arena (not in the free
  slot queue, kept alive by a fake invite), and teleports the viewer in as a
  spectator (same path as `.spect`).
- `Arena::PreUpdateImpl` drives it: recorded packets are resent on their
  original timeline; `MSG_MOVE_*` timestamps are shifted to the current server
  clock. The recorded players exist only on the viewer's client.
- after the last packet the viewer is sent back after 10 s.

## Seeking and pause

`Playback` keeps a timeline position (`positionMs`) instead of a start time; pausing stops it, the recorded
packets stay in memory so the timeline can be revisited. Pure logic: `ReplayTimeline.hpp` (tests in
`ReplayTimeline_test.cpp`).

- **Forward**: the packets between the current position and the target are sent at once, except one-shot
  effects (`isTransientOpcode`: spell start/go, combat log lines, visuals, sounds, emotes); of the movement
  packets (`MSG_MOVE_*`, `SMSG_MONSTER_MOVE`) only the last one per object is sent (`planJump`).
- **Back**: the client can't be rewound, so every recorded object the recording mentions (movers, aura targets,
  casters, destroyed objects - `collectObjectGuids`) is destroyed on the client (`SMSG_DESTROY_OBJECT`) and the
  timeline is rebuilt from 0 to the target the same way. Static objects that never move or cast (e.g. recorded
  doors) are not destroyed; their re-sent create packets just refresh them.
- Movement timestamps are rebased on every send (`now - recordStart - position`), so a jumped-to move looks as
  if it happened `position - offset` ago.
- **Pause** is approximate: the client keeps animating a movement spline that already started until it ends,
  and aura timers keep counting down client-side.
- At the end the viewer is sent back after 10 s unless paused; a seek back from the end resumes playing.

## Addon API

Commands (an addon sends them as chat: `SendChatMessage(".replay seek 95", "SAY")`):

| Command | Effect |
|---|---|
| `.replay pause` | pause |
| `.replay resume` | resume |
| `.replay seek <target>` | jump; `<target>` = seconds (`95`), `m:ss` (`1:35`), `+N` / `-N` relative seconds |
| `.replay state` | ask for a `STATE` message now |
| `.replay leave` | stop watching (sends `END` first) |

Server -> addon messages arrive like the ArenaSpectator ones: `CHAT_MSG_ADDON` with prefix `ACREPLAY`, i.e.
`event == "CHAT_MSG_ADDON"`, `arg1 == "ACREPLAY"`, `arg2` = the body below. All times are milliseconds.

| Body | When |
|---|---|
| `STATE;at=<pos>;len=<duration>;paused=<0/1>;` | on start, every second while watching, and right after pause / resume / seek / `.replay state` |
| `END;` | the replay closes (end reached and not paused, or `.replay leave`) |

The position only changes through playback and seeks, so a timeline UI can interpolate locally between
`STATE` messages while `paused=0`.

## Limitations

- A character can't watch a match it played in (its client would get a second
  copy of itself) - use another character. Fixing this needs GUID rewriting in
  update packets.
- What is seen is what the primary's client saw: enemies stealthed to the
  primary stay invisible.
- If the primary or secondary leaves the arena early, the recording loses that
  stream from then on.
- No seeking/pause; playback runs from the start at normal speed.
