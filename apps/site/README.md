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

Routing is client-side (React Router). The backend serves `index.html` for unknown non-asset
paths, so deep links and refreshes work in production.

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
`arena_team_member`, and `character_soloq_role`.

## Configuration

| Variable            | Default            |                                                |
| ------------------- | ------------------ | ---------------------------------------------- |
| `DATA_SOURCE`       | `dummy`            | `dummy` or `mysql`                             |
| `CHARACTER_DB_URL`  | required for mysql | `mysql://user:pass@host:3306/acore_characters` |
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