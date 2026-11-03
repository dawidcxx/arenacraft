import { describe, expect, test } from "bun:test";

import { EVERYONE, overwritesFor } from "./access";
import { LAYOUT, ROLES } from "./serverLayout";

const roleNames = new Set([EVERYONE, "vip", ...ROLES.map((role) => role.name)]);

describe("overwritesFor", () => {
  test("only the onboarding channel is visible to members without the Player role", () => {
    for (const category of LAYOUT)
      for (const channel of category.channels) {
        const everyone = overwritesFor(channel.access, channel.kind).find((o) => o.role === EVERYONE)!;
        if (channel.access.kind === "onboarding") expect(everyone.allow).toContain("ViewChannel");
        else expect(everyone.deny).toContain("ViewChannel");
      }
  });

  test("the onboarding channel allows commands but not chatting", () => {
    const everyone = overwritesFor({ kind: "onboarding" }, "text").find((o) => o.role === EVERYONE)!;
    expect(everyone.allow).toContain("UseApplicationCommands");
    expect(everyone.deny).toContain("SendMessages");
  });

  test("read-only channels let players read but only staff write", () => {
    const overwrites = overwritesFor({ kind: "players", readOnly: true }, "text");
    expect(overwrites.find((o) => o.role === "Player")!.deny).toContain("SendMessages");
    expect(overwrites.find((o) => o.role === "Game Master")!.allow).toContain("SendMessages");
  });

  test("language channels open with the language role, voice channels also allow connecting", () => {
    const ru = overwritesFor({ kind: "language", code: "ru" }, "text");
    expect(ru.find((o) => o.role === "Русский")!.allow).toEqual(["ViewChannel"]);
    const voice = overwritesFor({ kind: "players" }, "voice");
    expect(voice.find((o) => o.role === "Player")!.allow).toEqual(["ViewChannel", "Connect"]);
  });

  test("every role the layout refers to exists or is the existing vip role", () => {
    for (const category of LAYOUT)
      for (const channel of [category, ...category.channels])
        for (const overwrite of overwritesFor(channel.access, "text")) expect(roleNames.has(overwrite.role)).toBe(true);
  });
});
