import type { CategorySpec, ChannelKind, ChannelSpec, RoleSpec } from "./serverLayout";

// Pure planning for scripts/setup-discord.ts: compares the guild with the layout and lists what to do. Nothing is
// deleted, and only the default categories are renamed ("Kanały tekstowe" -> "📢 INFORMATION"); existing channels
// are moved and given permissions, so their history stays.

export interface SnapshotChannel {
  id: string;
  name: string;
  kind: ChannelKind | "category" | "other";
  parentId: string | null;
  topic: string | null;
}

export interface GuildSnapshot {
  roles: string[];
  channels: SnapshotChannel[];
}

export interface PlannedCategory {
  spec: CategorySpec;
  existing: SnapshotChannel | null; // null = create
  rename: boolean; // matched through an alias
}

export interface PlannedChannel {
  spec: ChannelSpec;
  category: CategorySpec;
  existing: SnapshotChannel | null; // null = create
  move: boolean; // existing, but in another category
  setTopic: boolean; // the layout has a topic and the channel has none
}

export interface Plan {
  createRoles: RoleSpec[];
  missingRoles: string[]; // referenced by access rules but neither present nor created (e.g. vip)
  categories: PlannedCategory[];
  channels: PlannedChannel[];
  unmanaged: SnapshotChannel[]; // channels the layout does not mention; left alone
}

const same = (a: string, b: string) => a.toLowerCase() === b.toLowerCase();

function matchesName(channel: SnapshotChannel, spec: { name: string; aliases?: string[] }): boolean {
  return same(channel.name, spec.name) || (spec.aliases ?? []).some((alias) => same(channel.name, alias));
}

function gameRoomOrder(name: string, pattern: RegExp): [number, number] {
  const match = pattern.exec(name);
  return [Number(match?.[1] ?? 0), Number(match?.[2] ?? 0)];
}

export function planSetup(snapshot: GuildSnapshot, layout: readonly CategorySpec[], roles: readonly RoleSpec[]): Plan {
  const createRoles = roles.filter((role) => !role.existing && !snapshot.roles.some((name) => same(name, role.name)));

  const knownRoles = new Set([...snapshot.roles, ...createRoles.map((role) => role.name)].map((n) => n.toLowerCase()));
  const missingRoles = new Set<string>();

  const categoryChannels = snapshot.channels.filter((channel) => channel.kind === "category");
  const usedCategories = new Set<string>();
  const categories: PlannedCategory[] = layout.map((spec) => {
    const existing =
      categoryChannels.find((c) => !usedCategories.has(c.id) && same(c.name, spec.name)) ??
      categoryChannels.find((c) => !usedCategories.has(c.id) && matchesName(c, spec)) ??
      null;
    if (existing) usedCategories.add(existing.id);
    return { spec, existing, rename: existing !== null && !same(existing.name, spec.name) };
  });

  const others = snapshot.channels.filter((channel) => channel.kind !== "category");
  const usedChannels = new Set<string>();
  const channels: PlannedChannel[] = [];

  for (const planned of categories) {
    if (planned.spec.access.kind === "role") missingRoleCheck(planned.spec.access.role);
    for (const spec of planned.spec.channels) {
      if (spec.access.kind === "role") missingRoleCheck(spec.access.role);

      const sameKind = (c: SnapshotChannel) => c.kind === spec.kind && !usedChannels.has(c.id);
      const matches = spec.pattern
        ? others
            .filter((c) => sameKind(c) && spec.pattern!.test(c.name))
            .sort((a, b) => {
              const [ag, as] = gameRoomOrder(a.name, spec.pattern!);
              const [bg, bs] = gameRoomOrder(b.name, spec.pattern!);
              return ag - bg || as - bs;
            })
        : others.filter((c) => sameKind(c) && matchesName(c, spec)).slice(0, 1);

      if (matches.length === 0 && !spec.pattern) {
        channels.push({ spec, category: planned.spec, existing: null, move: false, setTopic: false });
        continue;
      }
      for (const existing of matches) {
        usedChannels.add(existing.id);
        channels.push({
          spec,
          category: planned.spec,
          existing,
          move: existing.parentId === null || existing.parentId !== planned.existing?.id,
          setTopic: spec.kind !== "voice" && !!spec.topic && !existing.topic,
        });
      }
    }
  }

  return {
    createRoles,
    missingRoles: [...missingRoles],
    categories,
    channels,
    unmanaged: others.filter((c) => !usedChannels.has(c.id)),
  };

  function missingRoleCheck(role: string) {
    if (!knownRoles.has(role.toLowerCase())) missingRoles.add(role);
  }
}

/** Human readable dry-run output. */
export function describePlan(plan: Plan): string[] {
  const lines: string[] = [];
  for (const role of plan.createRoles) lines.push(`+ role      ${role.name}`);
  for (const role of plan.missingRoles) lines.push(`! role      ${role} is used by the layout but does not exist (not created)`);
  for (const category of plan.categories) {
    if (!category.existing) lines.push(`+ category  ${category.spec.name}`);
    else if (category.rename) lines.push(`~ category  ${category.existing.name} -> ${category.spec.name}`);
    else lines.push(`= category  ${category.spec.name}`);
    for (const channel of plan.channels.filter((c) => c.category === category.spec)) {
      const label = channel.existing?.name ?? channel.spec.name;
      const kind = channel.spec.kind === "text" ? "#" : channel.spec.kind === "forum" ? "forum " : "voice ";
      if (!channel.existing) lines.push(`    + ${kind}${label}`);
      else lines.push(`    ${channel.move ? "> move" : "= keep"} ${kind}${label}${channel.setTopic ? " (+topic)" : ""}`);
    }
  }
  for (const channel of plan.unmanaged) lines.push(`? not in the layout, left alone: ${channel.name}`);
  return lines;
}
