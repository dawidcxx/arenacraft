import type { AuthDb } from "./authDb";

export interface LegacyAccount {
  id: number;
  username: string;
}

export interface LegacyLinkRow {
  accountId: number;
  discordId: string;
}

export interface LegacyMember {
  id: string;
  username: string;
}

export interface PlannedLink {
  accountId: number;
  username: string;
  discordId: string;
}

export type LegacyConflictKind =
  | "no-member"
  | "member-already-linked"
  | "handle-reclaimed"
  | "multiple-members"
  | "link-failed";

export interface LegacyConflict {
  kind: LegacyConflictKind;
  account: LegacyAccount;
  discordId?: string;
  memberId?: string;
}

export interface LegacyLinkPlan {
  links: PlannedLink[];
  conflicts: LegacyConflict[];
}

export function planLegacyLinks(
  accounts: LegacyAccount[],
  links: LegacyLinkRow[],
  members: LegacyMember[],
): LegacyLinkPlan {
  const linkByAccount = new Map(links.map((link) => [link.accountId, link.discordId]));
  const linkedAccounts = new Map(links.map((link) => [link.discordId, link.accountId]));

  const membersByName = new Map<string, LegacyMember[]>();
  for (const member of members) {
    const key = member.username.toLowerCase();
    const group = membersByName.get(key) ?? [];
    group.push(member);
    membersByName.set(key, group);
  }

  const conflicts: LegacyConflict[] = [];
  const planned: PlannedLink[] = [];

  for (const account of accounts) {
    const linkedDiscordId = linkByAccount.get(account.id);
    if (!linkedDiscordId) continue;

    const matches = membersByName.get(account.username.toLowerCase()) ?? [];
    const other = matches.find((member) => member.id !== linkedDiscordId);
    if (other) {
      conflicts.push({
        kind: "handle-reclaimed",
        account,
        discordId: linkedDiscordId,
        memberId: other.id,
      });
    }
  }

  for (const account of accounts) {
    if (linkByAccount.has(account.id)) continue;

    const matches = membersByName.get(account.username.toLowerCase()) ?? [];
    if (matches.length === 0) {
      conflicts.push({ kind: "no-member", account });
      continue;
    }
    if (matches.length > 1) {
      conflicts.push({ kind: "multiple-members", account });
      continue;
    }

    const member = matches[0]!;
    if (linkedAccounts.has(member.id)) {
      conflicts.push({ kind: "member-already-linked", account, memberId: member.id });
      continue;
    }

    planned.push({ accountId: account.id, username: account.username, discordId: member.id });
    linkedAccounts.set(member.id, account.id);
  }

  planned.sort((a, b) => a.accountId - b.accountId);
  return { links: planned, conflicts };
}

export async function autoLinkLegacyAccounts(
  authDb: AuthDb,
  members: LegacyMember[],
): Promise<{ linked: number; plan: LegacyLinkPlan }> {
  const [accounts, links] = await Promise.all([authDb.listAccounts(), authDb.listDiscordLinks()]);
  const plan = planLegacyLinks(accounts, links, members);

  let linked = 0;
  for (const link of plan.links) {
    try {
      await authDb.bindDiscordId(link.accountId, link.discordId);
      linked++;
    } catch {
      plan.conflicts.push({
        kind: "link-failed",
        account: { id: link.accountId, username: link.username },
        memberId: link.discordId,
      });
    }
  }

  return { linked, plan };
}

export function formatLegacyConflict(conflict: LegacyConflict): string {
  const account = `\`${conflict.account.username}\` (#${conflict.account.id})`;

  switch (conflict.kind) {
    case "no-member":
      return `Legacy account ${account} matches no current Discord member; link manually or have the owner run /register old_password:<current password>.`;
    case "member-already-linked":
      return `Cannot auto-link ${account}: <@${conflict.memberId}> is already linked to another account.`;
    case "handle-reclaimed":
      return `Account ${account} is linked to <@${conflict.discordId}> but the handle is now held by <@${conflict.memberId}>; review for takeover.`;
    case "multiple-members":
      return `Cannot auto-link ${account}: multiple Discord members match that username.`;
    case "link-failed":
      return `Could not auto-link ${account} to <@${conflict.memberId}>; resolve manually.`;
  }
}
