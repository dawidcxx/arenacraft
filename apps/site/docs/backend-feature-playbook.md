# Backend Feature Construction Playbook (Agent Guide)

This guide defines how to add a backend feature in this repository.
Treat domain names as placeholders (users, documents, items, comments, etc.). This guide is intentionally domain-agnostic.

## 1) Start Boundary: Register A New Root Route

Root API composition lives in `packages/backend/src/shared/HttpServer.ts`, inside `buildRoutes()`.

Current pattern:

- Build two Hono instances: one app-level router and one API router.
- Mount feature routers into the API router with `api.route("/", featureApi.build())`.
- Mount API router into app with `app.route("/api", api)`.

For a new feature:

- Inject the new `{Feature}Api` into `HttpServer` constructor.
- Register it in `buildRoutes()` using the same root mount style.
- Keep feature route paths inside the feature contract/API class (for example `/users`, `/documents/:id`).

Why this matters:

- Every feature is composed at one backend root (`/api`).
- Feature routers remain independently testable and modular.

## 2) Canonical Backend Feature Shape

Use this folder shape under `packages/backend/src/features/{domain}`:

- `{Domain}Api.ts`: HTTP adapters, request parsing/validation, response status codes.
- `{Domain}Service.ts`: business logic and DB operations.
- `model/`: one or more Drizzle table files for the domain (`{entity}Table.ts` pattern). A single-table domain may keep one primary file such as `{domain}Table.ts`.
- `error/{domain}Errors.ts`: domain-specific errors annotated with HTTP metadata.

Suggested folder shape example:

- `packages/backend/src/features/{domain}/{Domain}Api.ts`
- `packages/backend/src/features/{domain}/{Domain}Service.ts`
- `packages/backend/src/features/{domain}/model/{entity}Table.ts`
- `packages/backend/src/features/{domain}/error/{domain}Errors.ts`

## 3) Responsibility Split (Important)

API layer (`{Domain}Api`):

- Use shared contract paths and schemas from common package.
- Parse request input with valibot (`v.parse(...)`) in handlers.
- Translate service outputs to HTTP responses and status codes.

Service layer (`{Domain}Service`):

- No HTTP concerns.
- No raw request parsing.
- Run business logic and data persistence.
- Throw domain errors for exceptional state.

Error mapping:

- Decorate domain errors with `@HttpError({ status })`.
- This feeds the global Hono error handler path.

## 4) Database Pattern

This app is a read-only proxy over an externally managed MySQL schema. It does not own or migrate
that schema.

- Add Drizzle table mappings under `model/` for the columns the feature reads.
- Put queries behind a feature repository interface, with MySQL and dummy implementations (see
  `packages/backend/src/features/ladder/`).
- Bind the implementation selected by configuration in `container.ts`.

If a future feature owns its storage, add migrations under `shared/migrations/` and run them during
startup.

## 5) DI Registration Pattern

Container wiring lives in `packages/backend/src/container.ts`.

Agent rule:

- Always check and update `packages/backend/src/container.ts` when adding or changing a backend feature so API classes, lifecycle hooks, and supporting dependencies stay registered.

For each new feature:

- Bind `{Feature}Api` in the container.
- Ensure any dependencies used by service/API classes are also bound.
- Keep shared infrastructure bindings in the shared section (config, DB, cache, migrator, server).

## 6) Lifecycle Hooks: OnInit And OnDeinit

Hook contracts:

- `packages/backend/src/shared/OnInit.ts`
- `packages/backend/src/shared/OnDeinit.ts`

Binding style:

- Multi-bind hook implementations via `provide: OnInit` or `provide: OnDeinit` with `multi: true` in container.

Execution points today:

- `OnDeinit` is executed in runtime shutdown flow (`packages/backend/src/main.ts`) on signals.
- `OnInit` exists as a contract and is executed in integration test bootstrap (`packages/backend/src/test/utils/integrationTestContext.ts`).

Practical guidance:

- Add `OnInit` when a feature needs explicit warm-up/bootstrapping.
- Add `OnDeinit` when a feature owns resources that must close cleanly.
- If runtime startup should execute `OnInit` globally, wire it explicitly in `main.ts` similarly to `OnDeinit` retrieval.

## 7) End Boundary: Expose Feature Through Shared API Contract

Shared API exposure pattern lives in common package contract files, using `{domain}Contract.ts` style.

Authoritative pattern location:

- `packages/common/src/api/{domain}Contract.ts`

For a new domain contract:

- Create `packages/common/src/api/{domain}Contract.ts`.
- Define valibot schemas for request/params/response.
- Export `{domain}Contract` object with method/path/request/params/response entries.
- Export inferred TypeScript types from those schemas.
- Re-export from `packages/common/src/api/index.ts`.

Contract shape example:

```ts
export const ladderContract = {
  soloq: {
    method: "GET",
    path: "/ladder/soloq",
    response: LadderResponse,
  },
} as const;
```

Use the same structure for any domain contract (for example `usersContract`, `documentsContract`, `itemsContract`).

Backend usage:

- Import contract constants and schemas in `{Domain}Api.ts`.
- Use contract `path` fields for Hono route registration.
- Parse inputs against the same shared schemas.

This is the endpoint of this guide: a feature is considered API-exposed when its shared contract is defined and exported from common API index.

## Quick Agent Checklist

- Route registered in `HttpServer.buildRoutes()` under `/api`.
- Feature API class bound in container.
- `packages/backend/src/container.ts` checked and updated for any feature wiring changes.
- API class validates/parses input and returns HTTP responses.
- Service class owns domain logic and persistence.
- Domain errors use `@HttpError` metadata.
- Migrations and one or more table mappings are added when persistence changes.
- Hook bindings are added when init/deinit behavior is needed.
- Shared `{domain}Contract.ts` exists and is exported from common API index.