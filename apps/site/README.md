# ArenaCraft Site

Read-only SoloQ ladder and landing site for the ArenaCraft website. Bun + Hono backend, Vite +
React frontend, and a MySQL ladder repository.

```
GET /api/ladder/soloq -> [{ "name", "class", "role", "rating", "wins", "losses" }, ...]
GET /api/health       -> { "ok": true }
```

The backend is a read-only proxy: it exposes the ladder as a plain array and rejects writes.
The frontend sorts nothing — the server returns the best rating first.

## Pages

| Route          | Content                                                                            |
| -------------- | ---------------------------------------------------------------------------------- |
| `/`            | Hero, next tourney, stats, community/Discord info, how to play, features, commands |
| `/tournaments` | Tourney schedule, prizes and rules                                                 |
| `/ladder`      | Live SoloQ ladder, filterable by role; refreshes every 60 seconds                  |
| `/streamers`   | Streamer zone and Twitch link                                                      |
| `/live-ladder` | Unlisted OBS widget: live top 5 over WebSocket                                     |

Routing is client-side (React Router). The backend serves `index.html` for unknown non-asset
paths, so deep links and refreshes work in production.

## Live ladder OBS widget

`/live-ladder` is an unlisted, chrome-less page meant to be added as an OBS browser source. It is
not linked from the site and uses its own layout (transparent page background, ~380px wide).

- Data arrives over `ws(s)://<host>/api/live-ladder/ws`.
- The server polls the top 5 once per second; every connected widget shares that one poller and
  polling stops when the last socket disconnects.
- Incremental `update` messages carry only changed ranks; a full `snapshot` is sent on connect and
  every 10 seconds.
- Player names use the account alias from the auth database (`account_alias`, set by the bot via
  `/alias set`), cached for 1 minute; characters without one fall back to the character name.
- At most 100 concurrent WebSockets; excess upgrades are rejected with `503` before the upgrade.
- `DATA_SOURCE=dummy` serves a drifting fixture ladder (`MockLadderTopService`) that simulates a
  rated game every 5-20 seconds, rotating forced rank swaps, top-5 knockouts and regular games.

### Reverse proxy

The widget needs a real WebSocket upgrade. nginx (and nginx-based edges) strip hop-by-hop headers by
default, so the location that reaches the backend must forward them:

```nginx
proxy_http_version 1.1;
proxy_set_header Upgrade $http_upgrade;
proxy_set_header Connection $connection_upgrade;
```

A global `proxy_set_header Connection "";` (part of NixOS `recommendedProxySettings`) has to be
overridden per location; NixOS does that with `proxyWebsockets = true;`. Without these headers the
backend receives a plain GET and answers `426 Upgrade Required`, and the widget retries every 2
seconds. In the backend log that shows up as a warning mentioning the missing `Upgrade` header.

`AUTH_DB_URL` also has to reach the container (for example `podman --env=AUTH_DB_URL`); without it
the live ladder works but shows character names instead of aliases.

## Assets

- Hero art, the Archivo subset and class icons live in `packages/web/src/assets/`.
- Class icons come from `brutaliccus/ClassicWoWClassIcons_Circle_HighRes`, resized to 96px WebP.
  That set has no Death Knight, so `death-knight.svg` is a VTracer vectorization of the WotLK
  class icon, clipped to the same circular frame.

## Development

```bash
bun install
bun run dev
```

- Web: http://localhost:5173
- Backend: http://localhost:8080
- Vite proxies `/api` to the backend.

Local development defaults to `DATA_SOURCE=dummy`, an in-memory fixture ladder. No database is
required: having Bun installed is enough.

## Data sources

The data source is chosen once at startup and injected through the `LadderRepository` interface.

| `DATA_SOURCE`     | Behaviour                                                                             |
| ----------------- | ------------------------------------------------------------------------------------- |
| `dummy` (default) | In-memory fixtures in `packages/backend/src/features/ladder/DummyLadderRepository.ts` |
| `mysql`           | Read-only MySQL query via `CHARACTER_DB_URL`                                          |

```bash
DATA_SOURCE=mysql \
CHARACTER_DB_URL=mysql://ladder:password@localhost:3306/acore_characters \
bun run dev:backend
```

Use a SELECT-only database account. The query reads `characters`, `arena_team`,
`arena_team_member`, and `character_soloq_role`. Set `AUTH_DB_URL` as well to read the bot-managed
`account_alias` table for the live ladder widget (character names are used without it).

## Configuration

| Variable            | Default            |                                                |
| ------------------- | ------------------ | ---------------------------------------------- |
| `DATA_SOURCE`       | `dummy`            | `dummy` or `mysql`                             |
| `CHARACTER_DB_URL`  | required for mysql | `mysql://user:pass@host:3306/acore_characters` |
| `AUTH_DB_URL`       | unset              | optional auth DB URL for `account_alias`       |
| `HOST` / `PORT`     | `0.0.0.0` / `8080` |                                                |
| `LADDER_SIZE`       | `200`              | 1-200 rows                                     |
| `CACHE_TTL_SECONDS` | `30`               | snapshot cache TTL                             |
| `DB_TIMEOUT_MS`     | `3000`             | per-query timeout                              |
| `CORS_ORIGINS`      | unset              | comma-separated allowed origins                |

The production image sets `DATA_SOURCE=mysql`; supply `CHARACTER_DB_URL` at deploy time.

## Build

```bash
bun run build:common
bun run build:web
bun run build:backend
```

`packages/backend/build.ts` copies the web build into `dist/public`, so the backend serves the
SPA and the API from one process.

## Project structure

- `packages/common`: shared domain types, valibot contracts, and the fetch client.
- `packages/backend`: Hono API, DI container, ladder repository implementations.
- `packages/web`: single Vite SPA entrypoint (`src/main.tsx`).