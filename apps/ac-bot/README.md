# ac-bot

Rewritten Arenacraft Discord bot. Six jobs (plus the server layout and onboarding below):

1. **Registration** - `/register` creates a Wow account (auth DB only) whose
   username is the caller's Discord handle and returns a generated password.
   Player accounts store their owner's immutable Discord user ID in
   `acore_auth.account_discord`, so renaming a handle cannot take over an
   account. Calling it again resets the password for that account. Legacy
   player accounts with no owner are reported by the startup audit and must be
   connected manually by an admin. Staff/GM accounts do not need a Discord
   owner.
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

## Server layout and onboarding

The server structure lives in code: `src/serverLayout.ts` (roles, categories, channels, who sees what).
`bun run setup:discord` compares the guild with it and prints a plan; `--apply` carries it out:

```bash
bun run setup:discord                          # dry run
bun run setup:discord --apply --sync-players   # create/move/permission + Player role for registered members
```

- Nothing is deleted and no channel is renamed, so message history stays. Existing channels are matched by name,
  moved into their category and get their permissions merged in (other overwrites, like the Waiting Room `Speak`
  deny, stay). Only the two default categories are renamed. Channels the layout does not know are left alone.
- Run it again any time; it only adds what is missing.
- `--sync-players` gives `Player` to members that already have a game account, so nobody loses access.
- The `game-<n>-<k>` voice rooms stay hidden (staff only); the bot opens a room to the players of a match
  while it lasts.
- The bot's role must be above the class/language/`Player` roles to hand them out; the script warns if not.
- The bot needs **Manage Roles**, **Manage Channels**, **Manage Threads**, **Create Private Threads** and
  **Move Members**.

Onboarding:

1. A new member sees only **#bot**. The pinned welcome panel (posted and kept up to date by the bot at startup)
   has language buttons (they toggle the `English`/`Русский`/`Deutsch`/`Français` roles and open
   `discussion-ru/de/fr`) and a class menu (one class role at a time, changeable).
2. `/register` sends the login, password and realmlist (`REALMLIST`) and gives `Player`, which opens the server.
3. **#support** has a pinned panel: **Open a ticket** creates a private thread with the staff, **Report a bug**
   opens the bug form.

Bug reports: `/bug` or the button opens a form (title, class/spell, what happened, steps, expected). It becomes a
post in the `bug-reports` forum tagged with the class and `Open`. Each report also goes to a `BugReportSink`
(`src/bugReport.ts`); it only logs today, a GitHub sink can create an issue per report later.

Roles: `Admin` (administrator), `Developer` and `Game Master` (moderation, see the staff category and tickets),
`vip` (existing), `Player`, one role per class (class colour) and per language.

## Setup

```bash
bun install
cp .env.example .env   # fill in the values
scripts/db_sync        # applies data/sql/updates/db_auth (account_discord)
bun run start
```

The bot needs the **Server Members** privileged intent enabled in the Discord
Developer Portal (Bot -> Privileged Gateway Intents). Members are resolved by
the Discord user ID stored in `account_discord`, never by their current
handle.

On startup the bot audits accounts: duplicate owners, reclaimed handles,
lookalike names and player accounts with no Discord owner. IP addresses are not
used for grouping; a proxy/VPN makes almost everyone share an IP.
`gm=<level>` marks staff accounts. Conflicts are left alone and must be resolved
manually, e.g.:

```sql
INSERT INTO acore_auth.account_discord (account_id, discord_id)
VALUES (<account id>, '<discord id>');
```

A player account without a Discord owner is an anomaly (legacy data or a ban),
not a normal state. Staff/GM accounts are exempt. List unowned player accounts
with:

```sql
SELECT a.id, a.username
FROM acore_auth.account a
LEFT JOIN acore_auth.account_discord d ON d.account_id = a.id
LEFT JOIN acore_auth.account_access aa ON aa.id = a.id AND aa.gmlevel > 0
WHERE d.account_id IS NULL AND aa.id IS NULL
ORDER BY a.id;
```

The bot is the only production entry point; there is no separate
account-migration script. (`scripts/emit-fake-event.ts` remains a dev-only Redis
helper for testing the voice flow.) The auth database URL comes from
`AUTH_DB_URL` or, when unset, the core's `AC_LOGIN_DATABASE_INFO`
(`host;port;user;password;database`).

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

Without `--team-a-ids` / `--team-b-ids` the bot resolves the fake `accountId`s
through `account_discord`; pass IDs for a purely fake match with no DB
accounts.
