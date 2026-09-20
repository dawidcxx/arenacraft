# ac-bot

Rewritten ArenaCraft Discord bot. Three jobs:

1. **Registration** - `/register` creates a Wow account (auth DB only) whose
   username is the caller's Discord handle and returns a generated password.
2. **Greeting** - `/ping` greets the caller publicly in the channel.
3. **Voice room shuffling** - subscribes to the core's `soloq-matchup` /
   `soloq-matchup-ended` Redis events and moves players between the waiting room
   and paired `game-<n>-<k>` voice channels.

On startup the bot overwrites both its global and guild command lists, so
commands removed from the code do not linger as unreachable "ghosts".

## Setup

```bash
bun install
cp .env.example .env   # fill in the values
bun run start
```

The bot needs the **Server Members** privileged intent enabled in the Discord
Developer Portal (Bot -> Privileged Gateway Intents). Members are resolved by
matching the Wow account name against the Discord username (case-insensitive).

`/register` is registered as a guild command, so it appears instantly.

### Voice channels

- `Waiting Room` - lobby everyone returns to.
- `game-<n>-1` / `game-<n>-2` - one room per `<n>`. Team A goes to `-1`, team B
  to `-2`. Rooms are discovered dynamically and reused once released.

Players without voice are skipped, so a room may hold fewer than six. If the bot
restarts, rooms that still contain members are treated as taken; a room held
longer than `STALE_ROOM_MINUTES` (default 60) is reclaimed.

## Fake events

Test the voice flow without the core:

```bash
# full cycle (publish matchup, wait, publish ended)
bun run scripts/emit-fake-event.ts --channel both --instance 42 \
  --team-a ALICE,BOB,CAROL --team-b DAVE,ERIN,FRANK

# just a matchup
bun run scripts/emit-fake-event.ts --channel matchup --instance 42

# just an end (test crash recovery / unknown instance)
bun run scripts/emit-fake-event.ts --channel ended --instance 42
```

Account names must match Discord usernames; defaults are provided.
