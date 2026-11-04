import { describe, expect, test } from "bun:test";

import { generatePassword } from "./credentials";

describe("generatePassword", () => {
  test("generates lowercase, unambiguous characters", () => {
    for (let i = 0; i < 20; i++) {
      expect(generatePassword()).toMatch(/^[a-hj-km-np-z2-9]{16}$/);
    }
  });

  test("honours a custom length", () => {
    expect(generatePassword(8)).toMatch(/^[a-hj-km-np-z2-9]{8}$/);
  });
});
