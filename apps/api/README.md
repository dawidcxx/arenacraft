# api

Read-only SoloQ ladder API for the website, protected with HTTP Basic auth. Bun + Hono + Drizzle
ORM (mysql2 driver).

```
GET /ladder/soloq   -> [{ "name", "class", "role", "rating", "wins", "losses" }, ...]  best rating first
GET /health         -> { "ok": true }
```

The API serves these paths at its root. The reverse proxy mounts it under `/api/` (so the public
paths are `/api/ladder/soloq` and `/api/health`); that prefix is the only routing the proxy does.

The ladder array is exactly what the site's `ladderApi` setting expects; the site sorts, filters by
role (Melee / Caster / Healer) and cuts to its `ladderSize` itself, so there is no pagination.

- Rows: every character with a 5v5 (soloq) arena team and at least one game this season, up to
  `LADDER_SIZE` (max 200).
- `role` comes from `character_soloq_role`, written by the worldserver whenever a character joins
  the soloq queue (`JoinSoloqQueue`, migration `data/sql/updates/db_characters/2026_11_02_01.sql`).
  Characters that have not queued since then get their class role if the class has only one
  (warrior, rogue, hunter, death knight -> Melee; mage, warlock -> Caster), otherwise `""`.

## Authentication

`/ladder/soloq` requires HTTP Basic auth. Set `AUTH_TOKENS` to one or more comma-separated tokens;
the compare is constant-time. A token may be sent as either the Basic username or password; the
other half is ignored, so both `user:<token>` and `<token>:` work.

```bash
curl -u client:dev-token http://localhost:8088/api/ladder/soloq
```

```js
fetch("https://ladder.example.com/api/ladder/soloq", {
  headers: { Authorization: "Basic " + btoa("client:dev-token") },
});
```

`/health` stays unauthenticated so container health checks do not need a token. Basic auth sends the
token in plaintext, so terminate TLS in front of the API.

## Hardening

The proxy is deliberately dumb: it only rewrites `/api/*` onto the service. Everything else lives
in the API or the container.

| Layer | What it does |
| --- | --- |
| api | HTTP Basic auth against `AUTH_TOKENS` (constant-time compare); in-memory snapshot (`CACHE_TTL_SECONDS`): concurrent misses share one query, the last good snapshot is served when the database is down; query strings ignored; per-query timeout (`DB_TIMEOUT_MS`); pool of 2 connections; 1 KB max body; errors never leak details |
| container | read-only root filesystem, all capabilities dropped, `no-new-privileges`, non-root user, memory/pid limits, tmpfs `/tmp` |
| database | use a SELECT-only account (below) |

### Read-only database account

```sql
CREATE USER 'ladder'@'%' IDENTIFIED BY '<strong password>';
GRANT SELECT ON acore_characters.characters           TO 'ladder'@'%';
GRANT SELECT ON acore_characters.arena_team           TO 'ladder'@'%';
GRANT SELECT ON acore_characters.arena_team_member    TO 'ladder'@'%';
GRANT SELECT ON acore_characters.character_soloq_role TO 'ladder'@'%';
```

Then set `CHARACTER_DB_URL=mysql://ladder:<password>@db:3306/acore_characters`.

## Running

```bash
podman compose --profile api up -d     # db + api + a small /api rewrite proxy on :8088
curl http://localhost:8088/api/health
curl -u client:dev-token http://localhost:8088/api/ladder/soloq
```

The compose file ships static development tokens (`dev-token` and `ops-token`); replace them with
real values in production. It only publishes the proxy on localhost. Put TLS in front of it
(Cloudflare Tunnel / proxied DNS or certbot); Basic auth credentials must not travel over plain
HTTP. Set `CORS_ORIGIN` to the site's origin to keep other sites from making authenticated
cross-origin requests.

## Configuration

| Variable | Default | |
| --- | --- | --- |
| `CHARACTER_DB_URL` | required | `mysql://user:pass@host:3306/acore_characters` |
| `AUTH_TOKENS` | required | comma-separated Basic auth tokens |
| `HOST` / `PORT` | `0.0.0.0` / `8080` | |
| `LADDER_SIZE` | `200` | 1-200 |
| `CACHE_TTL_SECONDS` | `30` | |
| `CORS_ORIGIN` | `*` | `Access-Control-Allow-Origin` value |
| `DB_TIMEOUT_MS` | `3000` | per query |

## Development

```bash
bun install
bun test
bun run typecheck
```

Build the standalone executable (used by the container image) with `bun run build`. The Dockerfile
compiles the API in a build stage and copies only that executable into the runtime image:

```bash
docker build -t arenacraft-api:local apps/api
```
