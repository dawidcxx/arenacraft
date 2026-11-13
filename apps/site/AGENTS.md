# AGENTS.md

## Dev server

Assume a dev server is already running at `http://localhost:5173` (Vite HMR) and the backend at
`http://localhost:8080`. Prompt the user to run `bun run dev` if not.

`bun run dev` starts:

- `bun run dev:backend` — Hono API server on `:8080`
- `bun run dev:web` — Vite dev server (TSX served directly, HMR)
- `bun run dev:common` — watch-mode rebuild of `packages/common`

The Vite dev server proxies `/api` to the backend.

## Data source

`DATA_SOURCE=dummy` (the default in `packages/backend/.env.local`) serves an in-memory fixture
ladder, so frontend work needs no database. `DATA_SOURCE=mysql` selects the read-only MySQL
repository and requires `CHARACTER_DB_URL`.

Repository selection happens in `packages/backend/src/container.ts`. Both implementations satisfy
`LadderRepository` in `packages/backend/src/features/ladder/LadderRepository.ts`.

## Adding a feature

Follow `docs/domain-modeling-playbook.md` first, then `docs/backend-feature-playbook.md`.

- Shared contract and domain types: `packages/common/src/api` and `packages/common/src/domain`.
- Backend feature: `packages/backend/src/features/{domain}`; bind the API class in
  `packages/backend/src/container.ts` and mount it in `HttpServer.buildRoutes()`.
- Frontend: single entrypoint at `packages/web/src/main.tsx`; API clients live in
  `packages/web/src/lib/apis.ts`.

## Routing

The frontend is one SPA served from `index.html`; client-side routes are `/`, `/tournaments`,
`/ladder` and `/streamers` (React Router in `packages/web/src/App.tsx`). Unknown non-asset paths
fall back to `index.html` in `packages/backend/src/lib/serveStaticAssets.ts`, so deep links work.
Pages live in `packages/web/src/pages/`, shared UI in `packages/web/src/components/`, and design
tokens (Archivo font, colors) in `packages/web/src/global.css`.

`/live-ladder` is an unlisted OBS widget rendered outside `SiteLayout`. It talks to
`/api/live-ladder/ws` (`packages/backend/src/features/liveLadder/`), is not linked from the site,
and must stay that way.

`/overlay/:token` is the personal OBS overlay of one streamer, also outside `SiteLayout` and
unlinked. The token comes from the Discord bot's `/overlay` command (auth DB `account_overlay`);
the page polls `/api/overlay/:token` (`packages/backend/src/features/playerOverlay/`), which reads
the character's SoloQ history from the core's arena replays (`arena_replay`). With
`DATA_SOURCE=dummy`, open `/overlay/dummyoverlaytoken000000000000000`.

## Frontend config

The frontend has no runtime config. Configuration is backend-only (`DATA_SOURCE`,
`CHARACTER_DB_URL`, `HOST`, `PORT`, etc.); the browser talks to the same-origin `/api` proxy.