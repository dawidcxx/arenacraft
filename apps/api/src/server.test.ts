import { describe, expect, test } from "bun:test";

import type { LadderEntry } from "./ladder";
import { createHandler, type HandlerDeps } from "./server";

const ENTRY: LadderEntry = { name: "Kalvax", class: "Rogue", role: "Melee", rating: 2412, wins: 184, losses: 61 };
const TOKEN = "test-token";

function handler(overrides: Partial<HandlerDeps> = {}) {
  return createHandler({
    ladder: async () => [ENTRY],
    corsOrigin: "https://site.example",
    cacheTtlSeconds: 30,
    authTokens: [TOKEN],
    ...overrides,
  });
}

function basic(credentials: string): string {
  return `Basic ${Buffer.from(credentials).toString("base64")}`;
}

function get(path: string, method = "GET", token: string | null = TOKEN) {
  const headers: Record<string, string> = {};
  if (token !== null) headers.Authorization = basic(`client:${token}`);
  return new Request(`http://api.local${path}`, { method, headers });
}

describe("api handler", () => {
  test("serves the ladder as a plain array with cors and cache headers", async () => {
    const response = await handler()(get("/ladder/soloq"));
    expect(response.status).toBe(200);
    expect(await response.json()).toEqual([ENTRY]);
    expect(response.headers.get("Access-Control-Allow-Origin")).toBe("https://site.example");
    expect(response.headers.get("Cache-Control")).toBe("private, max-age=30");
    expect(response.headers.get("X-Content-Type-Options")).toBe("nosniff");
  });

  test("requires basic auth", async () => {
    const response = await handler()(get("/ladder/soloq", "GET", null));
    expect(response.status).toBe(401);
    expect(response.headers.get("WWW-Authenticate")).toContain("Basic");
  });

  test("rejects an unknown token", async () => {
    expect((await handler()(get("/ladder/soloq", "GET", "wrong"))).status).toBe(401);
  });

  test("accepts a configured token as username or password", async () => {
    const asPassword = new Request("http://api.local/ladder/soloq", {
      headers: { Authorization: basic(`client:${TOKEN}`) },
    });
    const asUsername = new Request("http://api.local/ladder/soloq", {
      headers: { Authorization: basic(`${TOKEN}:`) },
    });
    expect((await handler()(asPassword)).status).toBe(200);
    expect((await handler()(asUsername)).status).toBe(200);
  });

  test("accepts any of the configured tokens", async () => {
    const api = handler({ authTokens: ["first-token", "second-token"] });
    expect((await api(get("/ladder/soloq", "GET", "second-token"))).status).toBe(200);
  });

  test("ignores query strings", async () => {
    let calls = 0;
    const response = await handler({
      ladder: async () => {
        calls++;
        return [ENTRY];
      },
    })(get("/ladder/soloq?limit=100000&bust=1"));
    expect(response.status).toBe(200);
    expect(calls).toBe(1);
  });

  test("answers preflight requests without auth", async () => {
    const response = await handler()(get("/ladder/soloq", "OPTIONS", null));
    expect(response.status).toBe(204);
    expect(response.headers.get("Access-Control-Allow-Methods")).toContain("GET");
    expect(response.headers.get("Access-Control-Allow-Headers")).toContain("Authorization");
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
    const response = await handler({
      ladder: async () => {
        throw new Error("ER_ACCESS_DENIED_ERROR: secret");
      },
    })(get("/ladder/soloq"));
    expect(response.status).toBe(503);
    expect(await response.text()).not.toContain("secret");
  });

  test("health check does not require auth", async () => {
    expect((await handler()(get("/health", "GET", null))).status).toBe(200);
  });
});
