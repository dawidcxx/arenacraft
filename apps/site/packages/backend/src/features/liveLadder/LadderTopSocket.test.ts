import { describe, expect, test } from "bun:test";
import type { ILadderTopService } from "./ILadderTopService";
import { LadderTopSocket } from "./LadderTopSocket";
import { WebSocketConnectionLimiter } from "./WebSocketConnectionLimiter";

const service: ILadderTopService = {
  subscriberCount: 0,
  subscribe: () => () => {},
};

describe("LadderTopSocket", () => {
  test("rejects a plain HTTP request before reserving a connection slot", async () => {
    const limiter = new WebSocketConnectionLimiter();
    const app = new LadderTopSocket(service, limiter).build();

    const response = await app.request("/ws");

    expect(response.status).toBe(426);
    expect(await response.json()).toMatchObject({ code: "UpgradeRequired" });
    expect(limiter.count).toBe(0);
  });
});