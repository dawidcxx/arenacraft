import { describe, expect, test } from "bun:test";

import { pingGreeting } from "./pingCommand";

describe("pingGreeting", () => {
  test("mentions the player", () => {
    expect(pingGreeting("<@123>")).toBe("Hello <@123>! Welcome to ArenaCraft.");
  });
});
