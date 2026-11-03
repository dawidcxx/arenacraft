export interface AccountLike {
  id: number;
  username: string;
  lastIp: string;
}

export interface LinkLike {
  accountId: number;
  discordId: string;
}

export interface MemberLike {
  id: string;
  username: string;
}

export interface AccountEntry {
  id: number;
  username: string;
  discordId: string | null;
  lastIp: string;
}

export type AccountFlagKind =
  | "duplicate-link"
  | "handle-reclaimed"
  | "member-already-linked"
  | "lookalike-name"
  | "orphaned"
  | "shared-ip";

export interface AccountFlag {
  kind: AccountFlagKind;
  title: string;
  accounts: AccountEntry[];
}

const IGNORED_IPS = new Set(["", "0.0.0.0", "127.0.0.1", "::1"]);

export function looseAccountKey(username: string): string {
  return username
    .normalize("NFKD")
    .replace(/[\u0300-\u036f]/g, "")
    .toLowerCase()
    .replace(/[^a-z0-9]/g, "");
}

function groupBy(entries: AccountEntry[], key: (entry: AccountEntry) => string | null): Map<string, AccountEntry[]> {
  const groups = new Map<string, AccountEntry[]>();
  for (const entry of entries) {
    const value = key(entry);
    if (value === null) continue;
    const group = groups.get(value);
    if (group) group.push(entry);
    else groups.set(value, [entry]);
  }
  return groups;
}

function byUsername(a: AccountEntry, b: AccountEntry): number {
  return a.username.localeCompare(b.username, undefined, { sensitivity: "base" }) || a.id - b.id;
}

export function buildAccountFlags(
  accounts: AccountLike[],
  links: LinkLike[],
  members: MemberLike[] = [],
): AccountFlag[] {
  const linkByAccount = new Map(links.map((link) => [link.accountId, link.discordId]));
  const entries: AccountEntry[] = accounts.map((account) => ({
    id: account.id,
    username: account.username,
    discordId: linkByAccount.get(account.id) ?? null,
    lastIp: account.lastIp,
  }));

  const flags: AccountFlag[] = [];

  for (const [discordId, group] of groupBy(entries, (entry) => entry.discordId)) {
    if (group.length < 2) continue;
    flags.push({
      kind: "duplicate-link",
      title: `Discord ID ${discordId} is linked to ${group.length} accounts`,
      accounts: [...group].sort(byUsername),
    });
  }

  for (const [key, group] of groupBy(entries, (entry) => looseAccountKey(entry.username) || null)) {
    if (group.length < 2) continue;
    flags.push({
      kind: "lookalike-name",
      title: `Account names collide after normalisation ("${key}")`,
      accounts: [...group].sort(byUsername),
    });
  }

  for (const [ip, group] of groupBy(entries, (entry) => (IGNORED_IPS.has(entry.lastIp) ? null : entry.lastIp))) {
    if (group.length < 2) continue;
    flags.push({
      kind: "shared-ip",
      title: `${group.length} accounts share last IP ${ip}`,
      accounts: [...group].sort(byUsername),
    });
  }

  if (members.length > 0) {
    const memberNames = new Set(members.map((member) => member.username.toLowerCase()));
    const orphaned = entries.filter(
      (entry) => entry.discordId === null && !memberNames.has(entry.username.toLowerCase()),
    );
    if (orphaned.length > 0) {
      flags.push({
        kind: "orphaned",
        title: `${orphaned.length} unlinked account(s) match no current Discord member (rename/claim risk)`,
        accounts: orphaned.sort(byUsername),
      });
    }

    const memberIdByUsername = new Map(members.map((member) => [member.username.toLowerCase(), member.id]));
    const reclaimed = entries.filter((entry) => {
      const currentMemberId = memberIdByUsername.get(entry.username.toLowerCase());
      return entry.discordId !== null && currentMemberId !== undefined && currentMemberId !== entry.discordId;
    });
    if (reclaimed.length > 0) {
      flags.push({
        kind: "handle-reclaimed",
        title: `${reclaimed.length} account(s) whose old handle is now held by a different Discord user`,
        accounts: reclaimed.sort(byUsername),
      });
    }

    const linkedDiscordIds = new Set(
      entries.map((entry) => entry.discordId).filter((id): id is string => id !== null),
    );
    const blocked = entries.filter((entry) => {
      if (entry.discordId !== null) return false;
      const memberId = memberIdByUsername.get(entry.username.toLowerCase());
      return memberId !== undefined && linkedDiscordIds.has(memberId);
    });
    if (blocked.length > 0) {
      flags.push({
        kind: "member-already-linked",
        title: `${blocked.length} unlinked account(s) match a Discord user who already owns another account`,
        accounts: blocked.sort(byUsername),
      });
    }
  }

  const order: AccountFlagKind[] = [
    "duplicate-link",
    "handle-reclaimed",
    "member-already-linked",
    "orphaned",
    "lookalike-name",
    "shared-ip",
  ];
  return flags.sort((a, b) => order.indexOf(a.kind) - order.indexOf(b.kind));
}
