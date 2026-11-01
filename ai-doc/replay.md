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
transaction as the insert); `KEEP_DAYS` also on worldserver startup. Per-player:
a match is deleted for all participants once it falls out of any one
participant's newest N.

## Commands (any player)

- `.replay <id>` / `.replay watch <id>` - watch
- `.replay list [name]` - newest 10, optionally only matches of `name`
- `.replay leave` - stop watching (`.spect leave` works too)

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

## Limitations

- A character can't watch a match it played in (its client would get a second
  copy of itself) - use another character. Fixing this needs GUID rewriting in
  update packets.
- What is seen is what the primary's client saw: enemies stealthed to the
  primary stay invisible.
- If the primary or secondary leaves the arena early, the recording loses that
  stream from then on.
- No seeking/pause; playback runs from the start at normal speed.
