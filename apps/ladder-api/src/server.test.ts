import { describe, expect, test } from "bun:test";

import type { LadderEntry } from "./ladder";
import { createHandler } from "./server";

const ENTRY: LadderEntry = { name: "Kalvax", class: "Rogue", role: "Melee", rating: 2412, wins: 184, losses: 61 };

function handler(ladder: () => Promise<LadderEntry[]> = async () => [ENTRY]) {
  return createHandler({ ladder, corsOrigin: "https://site.example", cacheTtlSeconds: 30 });
}

const get = (path: string, method = "GET") => new Request(`http://api.local${path}`, { method });

describe("ladder-api handler", () => {
  test("serves the ladder as a plain array with cors and cache headers", async () => {
    const response = await handler()(get("/ladder/soloq"));
    expect(response.status).toBe(200);
    expect(await response.json()).toEqual([ENTRY]);
    expect(response.headers.get("Access-Control-Allow-Origin")).toBe("https://site.example");
    expect(response.headers.get("Cache-Control")).toBe("public, max-age=30");
    expect(response.headers.get("X-Content-Type-Options")).toBe("nosniff");
  });

  test("ignores query strings", async () => {
    let calls = 0;
    const response = await handler(async () => {
      calls++;
      return [ENTRY];
    })(get("/ladder/soloq?limit=100000&bust=1"));
    expect(response.status).toBe(200);
    expect(calls).toBe(1);
  });

  test("answers preflight requests", async () => {
    const response = await handler()(get("/ladder/soloq", "OPTIONS"));
    expect(response.status).toBe(204);
    expect(response.headers.get("Access-Control-Allow-Methods")).toContain("GET");
  });

  test("rejects writes", async () => {
    for (const method of ["POST", "PUT", "DELETE", "PATCH"]) {
      const response = await handler()(get("/ladder/soloq", method));
      expect(response.status).toBe(405);
      expect(response.headers.get("Allow")).toBe("GET, HEAD, OPTIONS");
    }
  });

  test("unknown paths are 404", async () => {
    expect((await handler()(get("/admin"))).status).toBe(404);
  });

  test("database failures become a 503 without leaking details", async () => {
    const response = await handler(async () => {
      throw new Error("ER_ACCESS_DENIED_ERROR: secret");
    })(get("/ladder/soloq"));
    expect(response.status).toBe(503);
    expect(await response.text()).not.toContain("secret");
  });

  test("health check", async () => {
    expect((await handler()(get("/health"))).status).toBe(200);
  });
});
