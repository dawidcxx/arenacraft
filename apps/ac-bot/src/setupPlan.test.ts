import { describe, expect, test } from "bun:test";

import { LAYOUT, ROLES } from "./serverLayout";
import { describePlan, planSetup } from "./setupPlan";
import type { GuildSnapshot, SnapshotChannel } from "./setupPlan";

// The guild as it is today (two default categories, everything in them).
function currentGuild(): GuildSnapshot {
  const channels: SnapshotChannel[] = [
    { id: "cat-text", name: "Kanały tekstowe", kind: "category", parentId: null, topic: null },
    { id: "cat-voice", name: "Kanały głosowe", kind: "category", parentId: null, topic: null },
  ];
  const text = ["news", "general-discussion", "about", "bot", "ui-and-links", "changelog", "vip", "audit-log", "support"];
  for (const name of [...text, "discussion-ru", "discussion-de", "discussion-fr"])
    channels.push({ id: `t-${name}`, name, kind: "text", parentId: "cat-text", topic: null });
  const voice = ["Chill Room", "Waiting Room"];
  for (let n = 10; n >= 1; --n) for (const k of [2, 1]) voice.push(`game-${n}-${k}`);
  for (const name of voice) channels.push({ id: `v-${name}`, name, kind: "voice", parentId: "cat-voice", topic: null });
  return { roles: ["@everyone", "vip", "arenacraft"], channels };
}

describe("planSetup", () => {
  test("moves every existing channel instead of creating a new one", () => {
    const plan = planSetup(currentGuild(), LAYOUT, ROLES);
    const existingNames = currentGuild()
      .channels.filter((c) => c.kind !== "category")
      .map((c) => c.name);

    for (const name of existingNames) {
      const planned = plan.channels.filter((c) => c.existing?.name === name);
      expect(planned).toHaveLength(1);
    }
    expect(plan.channels.filter((c) => !c.existing).map((c) => c.spec.name)).not.toContain("support");
    expect(plan.unmanaged).toHaveLength(0);
  });

  test("reuses the default categories through their aliases", () => {
    const plan = planSetup(currentGuild(), LAYOUT, ROLES);
    const info = plan.categories.find((c) => c.spec.name === "📢 INFORMATION")!;
    const voice = plan.categories.find((c) => c.spec.name === "🔊 VOICE")!;
    expect(info.existing?.id).toBe("cat-text");
    expect(info.rename).toBe(true);
    expect(voice.existing?.id).toBe("cat-voice");
    // ui-and-links already sits in the text category, which becomes INFORMATION
    expect(plan.channels.find((c) => c.existing?.name === "ui-and-links")?.move).toBe(false);
    expect(plan.channels.find((c) => c.existing?.name === "general-discussion")?.move).toBe(true);
  });

  test("all game rooms are kept, in order, and none is created", () => {
    const plan = planSetup(currentGuild(), LAYOUT, ROLES);
    const rooms = plan.channels.filter((c) => c.spec.pattern);
    expect(rooms.map((c) => c.existing?.name)).toEqual(
      Array.from({ length: 10 }, (_, i) => [`game-${i + 1}-1`, `game-${i + 1}-2`]).flat(),
    );
    expect(rooms.every((c) => c.move === false)).toBe(true);
  });

  test("creates the missing roles but never the vip role", () => {
    const plan = planSetup(currentGuild(), LAYOUT, ROLES);
    const created = plan.createRoles.map((r) => r.name);
    expect(created).toContain("Admin");
    expect(created).toContain("Player");
    expect(created).toContain("Death Knight");
    expect(created).toContain("Русский");
    expect(created).not.toContain("vip");
    expect(plan.missingRoles).toEqual([]);

    const withoutVip = { ...currentGuild(), roles: ["@everyone"] };
    expect(planSetup(withoutVip, LAYOUT, ROLES).missingRoles).toEqual(["vip"]);
  });

  test("a second run changes nothing", () => {
    // simulate the guild after the first run
    const first = planSetup(currentGuild(), LAYOUT, ROLES);
    const roles = [...currentGuild().roles, ...first.createRoles.map((r) => r.name)];
    const channels: SnapshotChannel[] = first.categories.map((c, i) => ({
      id: `cat-${i}`,
      name: c.spec.name,
      kind: "category",
      parentId: null,
      topic: null,
    }));
    for (const planned of first.channels) {
      const parent = channels[first.categories.findIndex((c) => c.spec === planned.category)]!;
      channels.push({
        id: planned.existing?.id ?? `new-${planned.spec.name}`,
        name: planned.existing?.name ?? planned.spec.name,
        kind: planned.spec.kind,
        parentId: parent.id,
        topic: planned.spec.topic ?? null,
      });
    }

    const second = planSetup({ roles, channels }, LAYOUT, ROLES);
    expect(second.createRoles).toEqual([]);
    expect(second.categories.every((c) => c.existing && !c.rename)).toBe(true);
    expect(second.channels.every((c) => c.existing && !c.move && !c.setTopic)).toBe(true);
    expect(describePlan(second).some((line) => line.startsWith("+") || line.includes("> move"))).toBe(false);
  });

  test("channels the layout does not know are reported and left alone", () => {
    const guild = currentGuild();
    guild.channels.push({ id: "x", name: "memes", kind: "text", parentId: "cat-text", topic: null });
    const plan = planSetup(guild, LAYOUT, ROLES);
    expect(plan.unmanaged.map((c) => c.name)).toEqual(["memes"]);
  });
});
