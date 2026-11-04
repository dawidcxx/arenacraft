import type { AuthDb } from "./authDb";
import { buildAccountFlags } from "./duplicateAccounts";
import type { AccountFlag } from "./duplicateAccounts";
import type { MemberLike } from "./duplicateAccounts";
import { logger } from "./logger";

export function formatAccountFlag(flag: AccountFlag): string[] {
  const header = `[${flag.kind}] ${flag.title}`;
  const rows = flag.accounts.map(
    (account) => `  #${account.id}  ${account.username.padEnd(24)}  discord=${account.discordId ?? "missing"}  gm=${account.gmLevel}`,
  );
  return [header, ...rows];
}

export async function auditAccounts(authDb: AuthDb, members: MemberLike[]): Promise<void> {
  const accounts = await authDb.listAccounts();
  const flags = buildAccountFlags(accounts, members);
  if (flags.length === 0) {
    logger.info("Account audit: no duplicate or suspicious accounts found");
    return;
  }

  logger.warn(`Account audit: ${flags.length} group(s) need review`);
  for (const flag of flags) {
    for (const line of formatAccountFlag(flag)) logger.warn(line);
  }
}
