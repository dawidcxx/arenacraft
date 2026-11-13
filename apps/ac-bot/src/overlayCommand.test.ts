import { describe, expect, test } from "bun:test";

import { formatOverlayReply, makeOverlayToken, overlayUrl } from "./overlayCommand";

describe("makeOverlayToken", () => {
  test("is 32 url-safe characters, as the website expects", () => {
    expect(makeOverlayToken()).toMatch(/^[A-Za-z0-9_-]{32}$/);
  });

  test("is random per call", () => {
    expect(makeOverlayToken()).not.toBe(makeOverlayToken());
  });
});

describe("overlayUrl", () => {
  test("joins the base url and the token with a single slash", () => {
    expect(overlayUrl("http://arenacraft.fun/overlay", "abc")).toBe("http://arenacraft.fun/overlay/abc");
    expect(overlayUrl("http://arenacraft.fun/overlay//", "abc")).toBe("http://arenacraft.fun/overlay/abc");
  });
});

describe("formatOverlayReply", () => {
  test("shows the link with OBS setup hints", () => {
    const reply = formatOverlayReply("Strugly", "http://x/overlay/t", false);
    expect(reply.split("\n")).toEqual([
      "Your SoloQ overlay for **Strugly**:",
      "http://x/overlay/t",
      "Add it in OBS as a **Browser** source, 400 x 160. The background is transparent.",
      "Keep the link private. `/overlay` with `new_link: True` disables it and issues a new one.",
    ]);
  });

  test("says the old link stopped working when it was reissued", () => {
    expect(formatOverlayReply("Strugly", "http://x/overlay/t", true)).toStartWith(
      "New SoloQ overlay link for **Strugly**; the old link no longer works:",
    );
  });
});
