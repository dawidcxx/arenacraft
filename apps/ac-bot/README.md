# ac-bot

Rewritten Arenacraft Discord bot. Six jobs:

1. **Registration** - `/register` creates a Wow account (auth DB only) whose
   username is the caller's Discord handle and returns a generated password.
   Ownership is stored in `acore_auth.account_discord` against the immutable
   Discord user ID, so renaming a handle cannot take over an account. Calling it
   again resets the password for the account linked to that Discord ID. On
   startup the bot auto-links legacy accounts whose username still matches a
   current member; anything ambiguous is logged for manual resolution. An
   existing unlinked account can also be claimed by passing `old_password`.
2. **Greeting** - `/ping` greets the caller publicly in the channel.
3. **Voice room shuffling** - subscribes to the core's `soloq-matchup` /
   `soloq-matchup-ended` Redis events and moves players between the waiting room
   and paired `game-<n>-<k>` voice channels.
4. **Info commands** - `/toprankers` (soloQ leaderboard), `/onlinelist`
   (characters currently online) and `/lookup <character>` (the Discord user
   behind a character name). All reply ephemerally; the leaderboard and online
   list are served from a 30s cache to keep repeated calls off the DB.
5. **Match audit log** - posts one compact message per solo queue event to
   `AUDIT_LOG_CHANNEL_NAME` (default `audit-log`): match formed (teams, class
   icons, ratings), match finished (winner, duration, per-player rating
   change), plus aborted (invites declined) and cancelled-at-start
   (short-handed) matches.
6. **Aliases** - `/alias set` / `/alias reset`, restricted to the `VIP_ROLE_NAME`
   role. Stores a per-account display alias in `acore_auth.account_alias`; the
   in-game GM command `.alias get [character]` resolves it.

On startup the bot overwrites both its global and guild command lists, so
commands removed from the code do not linger as unreachable "ghosts".

## Setup

```bash
bun install
cp .env.example .env   # fill in the values
scripts/db_sync        # applies data/sql/updates/db_auth (account_discord)
bun run start
```

The bot needs the **Server Members** privileged intent enabled in the Discord
Developer Portal (Bot -> Privileged Gateway Intents). Members are resolved by
the Discord user ID stored in `account_discord`, never by their current handle.

On startup legacy accounts are auto-linked when exactly one current member still
holds the matching handle. Conflicts are logged and left alone: no matching
member, a member already linked to another account, or a linked account whose
old handle is now held by a different member. Resolve those manually, e.g.:

```sql
INSERT INTO acore_auth.account_discord (account_id, discord_id) VALUES (<account id>, '<discord id>');
```

To audit existing accounts for takeover/duplicate signals (duplicate links,
reclaimed handles, lookalike names, shared IPs, orphaned legacy accounts):

```bash
bun run flag-accounts
```

It prints a report and exits non-zero when anything is flagged. Pass no extra
flags; it reads the same `.env` as the bot.

The match audit log uses the guild's custom class emoji (`warrior`, `paladin`,
`hunter`, `rogue`, `priest`, `deathknight`, `shaman`, `mage`, `warlock`,
`druid`). Any missing emoji simply render as the bare character name. Set
`AUDIT_LOG_CHANNEL_NAME` to an empty string to disable these messages.

`/alias` is registered with `default_member_permissions: 0`, so it is hidden
from regular members. Allow the `VIP_ROLE_NAME` role once in **Server Settings
-> Integrations -> arenacraft -> Manage -> /alias**; Discord only accepts an
OAuth2 bearer token for that overwrite, so the bot cannot set it itself.
Registration is an upsert by command name, so the overwrite survives restarts.

The in-game GM command `.alias get [character]` resolves character -> account
-> alias and prints one machine-readable line for future addons:
`ALIAS:<character>=<alias-or-account>` (the account name is hidden whenever an
alias is set).

`/register` is registered as a guild command, so it appears instantly.

### Voice channels

- `Waiting Room` - lobby everyone returns to. The bot denies `Speak` on the
  channel so the queue stays quiet; channel-scoped means it cannot leave anyone
  server-muted outside the lobby.
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

# move real Discord users into the rooms (IDs from Discord, no DB lookup)
bun run scripts/emit-fake-event.ts --channel both --instance 42 \
  --team-a ALICE,BOB,CAROL --team-b DAVE,ERIN,FRANK \
  --team-a-ids 111111111111111111,222222222222222222,333333333333333333 \
  --team-b-ids 444444444444444444,555555555555555555,666666666666666666

# just a matchup
bun run scripts/emit-fake-event.ts --channel matchup --instance 42

# just an end (test crash recovery / unknown instance)
bun run scripts/emit-fake-event.ts --channel ended --instance 42
```

Without `--team-a-ids` / `--team-b-ids` the fake `accountId`s are looked up in
`account_discord`; pass IDs for a purely fake match with no DB accounts.
