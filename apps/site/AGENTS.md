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

There is one SPA: `/` → `index.html`, declared in `packages/common/src/consts/pages.ts`.
Production static serving lives in `packages/backend/src/lib/serveStaticAssets.ts`.

## Frontend config

The frontend has no runtime config. Configuration is backend-only (`DATA_SOURCE`,
`CHARACTER_DB_URL`, `HOST`, `PORT`, etc.); the browser talks to the same-origin `/api` proxy.