# api

Read-only SoloQ ladder API for the website, protected with HTTP Basic auth. Bun + mysql2, no framework.

```
GET /ladder/soloq   -> [{ "name", "class", "role", "rating", "wins", "losses" }, ...]  best rating first
GET /health         -> { "ok": true }
```

The array is exactly what the site's `ladderApi` setting expects; the site sorts, filters by role
(Melee / Caster / Healer) and cuts to its `ladderSize` itself, so there is no pagination.

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
curl -u client:dev-token http://localhost:8088/ladder/soloq
```

```js
fetch("https://ladder.example.com/ladder/soloq", {
  headers: { Authorization: "Basic " + btoa("client:dev-token") },
});
```

`/health` stays unauthenticated so container health checks do not need a token. Basic auth sends the
token in plaintext, so always put TLS in front of the nginx port (see [Running](#running)).

## Hardening

| Layer | What it does |
| --- | --- |
| nginx (`nginx/api.conf`) | micro-cache keyed on the path + `Authorization` (one entry per token; query strings cannot bust it) with `proxy_cache_lock` and stale-on-error; GET/OPTIONS only; 1 KB bodies, 5 s client timeouts |
| api | HTTP Basic auth against `AUTH_TOKENS` (constant-time compare); in-memory snapshot (`CACHE_TTL_SECONDS`): concurrent misses share one query, the last good snapshot is served when the database is down; query strings ignored; 3 s query timeout; pool of 2 connections; 1 KB max body; errors never leak details |
| container | read-only root filesystem, all capabilities dropped, `no-new-privileges`, non-root user, memory/pid limits; only nginx publishes a port |
| database | use a SELECT-only account (below) |

### Read-only database account

```sql
CREATE USER 'ladder'@'%' IDENTIFIED BY '<strong password>';
GRANT SELECT ON acore_characters.characters           TO 'ladder'@'%';
GRANT SELECT ON acore_characters.arena_team           TO 'ladder'@'%';
GRANT SELECT ON acore_characters.arena_team_member    TO 'ladder'@'%';
GRANT SELECT ON acore_characters.character_soloq_role TO 'ladder'@'%';
```

Then set `CHARACTER_DB_URL=mysql://ladder:<password>@db:3306/acore_characters` for `api` in
`docker-compose.yml`.

## Running

```bash
podman compose --profile api up -d     # db + api + api-nginx on :8088
```

The compose file ships static development tokens (`dev-token` and `ops-token`); replace them with
real values in production:

```bash
curl -u client:dev-token http://localhost:8088/ladder/soloq
```

The website is served over https, so browsers refuse a plain `http://` API. Put TLS in front of
port 8088 with a Cloudflare Tunnel / proxied DNS record or certbot; Basic auth credentials must not
travel over plain HTTP. Set `CORS_ORIGIN` to the site's origin to keep other sites from making
authenticated cross-origin requests.

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
