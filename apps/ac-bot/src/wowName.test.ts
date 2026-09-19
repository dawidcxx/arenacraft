import { describe, expect, test } from "bun:test";

import { normalizeAccountName } from "./wowName";

describe("normalizeAccountName", () => {
  test("passes the Discord username through, uppercased", () => {
    expect(normalizeAccountName("john99")).toBe("JOHN99");
    expect(normalizeAccountName("Alpha")).toBe("ALPHA");
    expect(normalizeAccountName("john_doe")).toBe("JOHN_DOE");
    expect(normalizeAccountName("john.doe")).toBe("JOHN.DOE");
    expect(normalizeAccountName("john-doe")).toBe("JOHN-DOE");
  });
});
