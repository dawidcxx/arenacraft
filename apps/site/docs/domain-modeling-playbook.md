# Domain Modeling Playbook

Domain-first design. Model the domain before writing backend features, database tables, or UI. The domain is the single source of truth that everything else projects from.

## Workflow: design the domain with an LLM

Before adding a feature, describe the domain to your LLM and iterate on the model first. Ask the LLM to write domain types in `packages/common/src/domain/`. Once the domain shape is solid, follow the backend feature playbook and API contract patterns — they all reference the domain root.

## Structure

```
packages/common/src/domain/
├── index.ts              # barrel: re-exports core + all feature domains
├── core/
│   ├── index.ts           # barrel for core types
│   ├── Uuid.ts            # branded UUID type, valibot schema, helpers
│   ├── JsDate.ts          # Date-safe valibot schema (survives JSON round-trip)
│   └── JsonObject.ts      # recursive JSON type (use sparingly)
└── todo/                  # feature domain
    ├── index.ts           # barrel: re-exports domain root
    └── Todo.ts            # domain root: Todo entity
```

## Core types (`domain/core/`)

Foundation types used across feature domains. Add new leaf value types here when they represent a reusable primitive — for example, an ISO-8601 string type with its own valibot schema.

Existing core types:

- **`Uuid`** — branded `string & { __type: "UUID" }` with valibot validation and helpers (`generateId`, `uuidFromString`, `isUuid`, etc.)
- **`JsDate`** — valibot schema that deserializes strings/numbers into `Date` objects, so dates survive JSON round-trips between backend and frontend
- **`JsonObject`** — recursive JSON type; prefer structured domain types over this unless the data is truly opaque

## Feature domain roots

Each feature gets its own folder: `domain/{feature}/`. The domain root is a valibot object schema — the single source of truth for both runtime validation and compile-time types.

### Example (`domain/todo/Todo.ts`)

```ts
import { boolean, InferOutput, minLength, object, pipe, string } from "valibot";
import { Uuid } from "../core";
import { JsDate } from "../core/JsDate";

export const Todo = object({
  id: Uuid,
  content: pipe(string(), minLength(1)),
  completed: boolean(),
  createdAt: JsDate,
});

export type Todo = InferOutput<typeof Todo>;
```

Key patterns to follow:

1. **Schema and type share the same name** — `export const Todo = object(...)` and `export type Todo = InferOutput<typeof Todo>`. This is the idiomatic valibot pattern and avoids naming friction.

2. **Use core types** — reach for `Uuid`, `JsDate`, etc. from `domain/core/` before defining new primitives. Only add a new leaf value type to `core/` when the existing ones don't cover the semantics (e.g., an ISO 8601 string type).

3. **Domain roots are trees** — the top-level object should be directly serializable via `JSON.stringify` / `JSON.parse`. No circular references, no class instances (other than `Date` which `JsDate` handles). This ensures seamless transfer between backend and frontend over HTTP.

4. **Utility functions belong to the domain** — add helper functions alongside the domain root when they express domain logic (e.g., `isOverdue(todo)`, `displayName(user)`). These live in the same file or a sibling file in the domain folder.

## How the domain flows

```
domain/ (common package)
    │
    ├── api/ contracts reference domain types for request/response shapes
    │
    ├── backend features map DB rows → domain roots
    │       └── See ladder.ts toLadderEntry() for the pattern
    │
    └── frontend consumes domain types directly via imports
```

### Backend mapping pattern

The backend maps its internal DB representation into the domain root. A plain function (not a class method) does the transformation:

```ts
function mapTodo(dbTodo: DbTodo): Todo {
  return {
    id: dbTodo.id,
    content: dbTodo.content,
    completed: dbTodo.completed,
    createdAt: dbTodo.createdAt,
  };
}
```

`DbTodo` is the Drizzle-inferred persistence type. `Todo` is the domain root. The service layer returns domain types, never raw DB rows.

## Don't overcook it

- Start with the smallest useful domain shape. Add fields as needed.
- Don't pre-model every possible edge case up front.
- Domain roots don't need classes, repositories, or heavy abstraction — valibot objects and plain functions are enough.
- Prefer explicit mapping functions over ORM magic or automatic serialization layers.