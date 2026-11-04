export interface AccountLike {
  id: number;
  username: string;
  discordId: string | null;
  gmLevel?: number;
}

export interface MemberLike {
  id: string;
  username: string;
}

export interface AccountEntry {
  id: number;
  username: string;
  discordId: string | null;
  gmLevel: number;
}

export type AccountFlagKind =
  | "duplicate-owner"
  | "handle-reclaimed"
  | "member-already-owns-account"
  | "lookalike-name"
  | "unconnected";

export interface AccountFlag {
  kind: AccountFlagKind;
  title: string;
  accounts: AccountEntry[];
}

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

export function buildAccountFlags(accounts: AccountLike[], members: MemberLike[] = []): AccountFlag[] {
  const entries: AccountEntry[] = accounts.map((account) => ({
    id: account.id,
    username: account.username,
    discordId: account.discordId,
    gmLevel: account.gmLevel ?? 0,
  }));

  const flags: AccountFlag[] = [];

  for (const [discordId, group] of groupBy(entries, (entry) => entry.discordId)) {
    if (group.length < 2) continue;
    flags.push({
      kind: "duplicate-owner",
      title: `Discord ID ${discordId} owns ${group.length} accounts`,
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

  if (members.length > 0) {
    const memberNames = new Set(members.map((member) => member.username.toLowerCase()));
    const unconnected = entries.filter(
      (entry) =>
        entry.gmLevel === 0 && entry.discordId === null && !memberNames.has(entry.username.toLowerCase()),
    );
    if (unconnected.length > 0) {
      flags.push({
        kind: "unconnected",
        title: `${unconnected.length} account(s) have no Discord owner and match no current member`,
        accounts: unconnected.sort(byUsername),
      });
    }

    const memberIdByUsername = new Map(members.map((member) => [member.username.toLowerCase(), member.id]));
    const reclaimed = entries.filter((entry) => {
      const currentMemberId = memberIdByUsername.get(entry.username.toLowerCase());
      return (
        entry.gmLevel === 0 &&
        entry.discordId !== null &&
        currentMemberId !== undefined &&
        currentMemberId !== entry.discordId
      );
    });
    if (reclaimed.length > 0) {
      flags.push({
        kind: "handle-reclaimed",
        title: `${reclaimed.length} account(s) whose old handle is now held by a different Discord user`,
        accounts: reclaimed.sort(byUsername),
      });
    }

    const ownedDiscordIds = new Set(
      entries.map((entry) => entry.discordId).filter((id): id is string => id !== null),
    );
    const blocked = entries.filter((entry) => {
      if (entry.gmLevel !== 0 || entry.discordId !== null) return false;
      const memberId = memberIdByUsername.get(entry.username.toLowerCase());
      return memberId !== undefined && ownedDiscordIds.has(memberId);
    });
    if (blocked.length > 0) {
      flags.push({
        kind: "member-already-owns-account",
        title: `${blocked.length} account(s) have no Discord owner but match a Discord user who already owns another account`,
        accounts: blocked.sort(byUsername),
      });
    }
  }

  const order: AccountFlagKind[] = [
    "duplicate-owner",
    "handle-reclaimed",
    "member-already-owns-account",
    "unconnected",
    "lookalike-name",
  ];
  return flags.sort((a, b) => order.indexOf(a.kind) - order.indexOf(b.kind));
}
