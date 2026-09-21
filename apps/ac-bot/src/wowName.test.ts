import { describe, expect, test } from "bun:test";

import { normalizeAccountName } from "./wowName";

describe("normalizeAccountName", () => {
  test("passes the Discord username through, lowercased", () => {
    expect(normalizeAccountName("john99")).toBe("john99");
    expect(normalizeAccountName("Alpha")).toBe("alpha");
    expect(normalizeAccountName("John_Doe")).toBe("john_doe");
    expect(normalizeAccountName("john.doe")).toBe("john.doe");
    expect(normalizeAccountName("john-doe")).toBe("john-doe");
    expect(normalizeAccountName("  spaced  ")).toBe("spaced");
  });
});
