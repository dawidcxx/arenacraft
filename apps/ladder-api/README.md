# ladder-api

Read-only public SoloQ ladder for the website. Bun + mysql2, no framework.

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

## Hardening

| Layer | What it does |
| --- | --- |
| nginx (`nginx/ladder-api.conf`) | 2 req/s per IP (burst 20) and 10 connections per IP, else `429`; micro-cache keyed on the path only (query strings cannot bust it) with `proxy_cache_lock` and stale-on-error; GET/OPTIONS only; 1 KB bodies, 5 s client timeouts |
| ladder-api | in-memory snapshot (`CACHE_TTL_SECONDS`): concurrent misses share one query, the last good snapshot is served when the database is down; query strings ignored; 3 s query timeout; pool of 2 connections; 1 KB max body; errors never leak details |
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

Then set `CHARACTER_DB_URL=mysql://ladder:<password>@db:3306/acore_characters` for `ladder-api` in
`docker-compose.yml`.

## Running

```bash
podman compose --profile ladder up -d     # db + ladder-api + ladder-nginx on :8088
```

The website is served over https, so browsers refuse a plain `http://` API. Put TLS in front of
port 8088: a Cloudflare Tunnel / proxied DNS record (then uncomment the `set_real_ip_from` /
`real_ip_header CF-Connecting-IP` block in the nginx config so limits stay per visitor) or certbot.
Set `CORS_ORIGIN` to the site's origin to keep other sites from embedding the ladder.

## Configuration

| Variable | Default | |
| --- | --- | --- |
| `CHARACTER_DB_URL` | required | `mysql://user:pass@host:3306/acore_characters` |
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
