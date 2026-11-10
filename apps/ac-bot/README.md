# ac-bot

The Arenacraft Discord bot. It handles account self-service, solo queue voice rooms, info
commands and the match audit log. Bun + discord.js; it talks to the auth/characters MySQL
databases and to Valkey/Redis.

## Commands

| Command | Who | What |
| --- | --- | --- |
| `/register` | everyone | Creates a WoW account named after the Discord handle and DMs the generated password. Running it again resets the password. |
| `/setpassword` | everyone | Sets a new password for the caller's account. |
| `/ping` | everyone | Public greeting in the channel. |
| `/toprankers` | everyone | Top 10 SoloQ players (30s cache). |
| `/onlinelist` | everyone | Characters currently online (30s cache). |
| `/lookup <character>` | everyone | The Discord user behind a character name. |
| `/charlist <discord>` | VIP | Characters owned by a Discord member. |
| `/alias set/reset` | VIP | Solo queue display alias; resolved in game by `.alias get`. |
| `/permaban <discord_id> <reason>` | VIP | Permanently bans the Discord user and their linked core account. |
| `/baninfo <discord_acc>` | VIP | Current core ban and Discord server ban status for a user. |
| `/myseat` | everyone | Your tournament seating tier (`None`, `Gold` or `Diamond`), 60s cache. |
| `/setseat <discord_id> <tier>` | VIP | Set a linked player's seating tier (clears the list cache). |
| `/getseatlist <tier>` | everyone | Padded list of `Gold` or `Diamond` seats, newest first, 60s cache. |

Replies to info commands are ephemeral. On startup the bot replaces the guild command list,
so removed commands do not linger as unreachable "ghosts".

Accounts are owned by Discord user ID (`acore_auth.account_discord`), not by handle, so
renaming a Discord account cannot take over a WoW account. Staff/GM accounts are exempt from
ownership checks. Player accounts without an owner are reported by the startup audit and must
be linked by an admin:

```sql
INSERT INTO acore_auth.account_discord (account_id, discord_id)
VALUES (<account id>, '<discord id>');
```

## Setup

```bash
bun install
cp .env.example .env   # fill in the values
ac db-update           # applies data/sql/updates/db_auth (account_discord/account_alias/account_seat)
bun run start
```

The bot needs the **Server Members** privileged intent enabled in the Discord Developer Portal
(Bot -> Privileged Gateway Intents).

`AUTH_DB_URL` may be omitted when the core's `AC_LOGIN_DATABASE_INFO`
(`host;port;user;password;database`) is present; the bot derives the URI from it.

## Configuration

| Variable | Default | |
| --- | --- | --- |
| `DISCORD_TOKEN` | required | Bot token from the Developer Portal. |
| `DISCORD_GUILD_ID` | required | The single guild the bot serves. |
| `AUTH_DB_URL` | required* | `mysql://user:pass@host:3306/acore_auth` |
| `CHARACTER_DB_URL` | required | `mysql://user:pass@host:3306/acore_characters` |
| `REDIS_URL` | `redis://127.0.0.1:6379` | Core event stream. |
| `WAITING_ROOM_NAME` | `Waiting Room` | Lobby everyone returns to. |
| `GAME_CHANNEL_PATTERN` | `^game-(\d+)-(\d+)$` | Paired voice room names. |
| `INFO_CHANNEL_NAME` | `bot` | Channel for "no free room" notices. |
| `AUDIT_LOG_CHANNEL_NAME` | `audit-log` | Match audit messages; empty disables them. |
| `VIP_ROLE_NAME` | `vip` | Role allowed to use `/alias`, `/charlist`, `/permaban`, `/baninfo` and `/setseat`. |
| `STALE_ROOM_MINUTES` | `60` | Reclaim a room whose ended event never arrived. |

\* optional when `AC_LOGIN_DATABASE_INFO` is set.

`/alias`, `/charlist`, `/permaban`, `/baninfo` and `/setseat` are registered with
`default_member_permissions: 0`, so they are hidden from regular members. Allow the VIP role
once in **Server Settings -> Integrations -> arenacraft -> Manage -> /alias** (and `/charlist`,
`/permaban`, `/baninfo` and `/setseat`); Discord only accepts an OAuth2 bearer token for those
overwrites, so the bot cannot set them itself. Registration is an upsert by command name, so the
overwrites survive restarts.

Tournament seats (`/myseat`, `/setseat`, `/getseatlist`) live in the auth DB's
`account_seat` table keyed by linked account id; a missing row means `None`. `reset_soloq`
reads them to seed Gold/Diamond players at a higher starting MMR for the next season.

`/permaban` opens a confirmation modal showing the target and reason; type `BAN` to apply it.
It writes a permanent row to `acore_auth.account_banned` (a ban is permanent when
`unbandate = bandate`) and needs the bot's **Ban Members** permission to remove the user from
the guild. An unlinked Discord user is still banned from Discord. The core checks the ban at
login, so a character already in the world keeps playing until it disconnects or relogs.

`/baninfo` shows the current active core ban (permanent or temporary) from `account_banned`
and checks the guild's Discord ban list for the same user.

## Voice rooms

The bot subscribes to the core's `soloq-matchup` / `soloq-matchup-ended` Redis events:

- `Waiting Room` - lobby everyone returns to. The bot denies `Speak` there so the queue stays
  quiet; the denial is channel-scoped, so it cannot server-mute anyone outside the lobby.
- `game-<n>-1` / `game-<n>-2` - one room per `<n>`. Team A goes to `-1`, team B to `-2`.
  Rooms are discovered dynamically and reused once released.

Players without voice are skipped, so a room may hold fewer than six. If the bot restarts,
rooms that still contain members are treated as taken; a room held longer than
`STALE_ROOM_MINUTES` is reclaimed.

The in-game GM command `.alias get [character]` resolves character -> account -> alias and
prints one machine-readable line: `ALIAS:<character>=<alias-or-account>`.

## Audit log

One compact message per event is posted to `AUDIT_LOG_CHANNEL_NAME`:

- match formed - instance, arena type and map, both teams with class icons and ratings;
- match finished - winner, duration and per-player rating change;
- aborted - invites declined;
- cancelled at start - team short-handed.

The class icons come from the guild's custom emoji (`warrior`, `paladin`, `hunter`, `rogue`,
`priest`, `deathknight`, `shaman`, `mage`, `warlock`, `druid`). Missing emoji simply render as
the bare character name.

## Docker

The image compiles the bot to a single executable and runs it as an unprivileged user:

```bash
docker build -t arenacraft-ac-bot:local apps/ac-bot
```

It reads `DISCORD_TOKEN` and `DISCORD_GUILD_ID` from `.env`. The CI workflow
`ac-bot-image.yml` publishes `ghcr.io/dawidcxx/arenacraft-bot:latest` on pushes to `local`
that touch `apps/ac-bot`. The `ac-bot-deploy` workflow is the manual, `local`-only
deployment hook (currently a placeholder for the downstream webhook); it verifies the
requested image tag exists before running.

## Development

```bash
bun test
bun run typecheck
```
