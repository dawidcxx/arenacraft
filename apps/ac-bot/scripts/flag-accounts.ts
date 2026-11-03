import { AuthDb } from "../src/authDb";
import { getConfig } from "../src/config";
import { buildAccountFlags } from "../src/duplicateAccounts";
import type { MemberLike } from "../src/duplicateAccounts";
import { autoLinkLegacyAccounts, formatLegacyConflict } from "../src/legacyLink";

const USAGE = `Usage: bun run flag-accounts [--link] [--shared-ip] [--strict]

  --link        auto-link legacy accounts by current handle before reporting
  --shared-ip   include shared-IP groups (usually noise behind a NAT/VPN)
  --strict      exit with code 1 when flags are found (default: exit 0)`;

async function fetchGuildMembers(guildId: string, token: string): Promise<MemberLike[]> {
  const members: MemberLike[] = [];
  let after: string | null = null;

  for (;;) {
    const url = new URL(`https://discord.com/api/v10/guilds/${guildId}/members`);
    url.searchParams.set("limit", "1000");
    if (after) url.searchParams.set("after", after);

    const response = await fetch(url, { headers: { Authorization: `Bot ${token}` } });
    if (!response.ok) {
      throw new Error(`Discord member fetch failed (${response.status}): ${await response.text()}`);
    }

    const page = (await response.json()) as { user: { id: string; username: string; bot?: boolean } }[];
    for (const member of page) {
      if (member.user.bot) continue;
      members.push({ id: member.user.id, username: member.user.username });
    }

    if (page.length < 1000) break;
    after = page[page.length - 1]!.user.id;
  }

  return members;
}

async function main(): Promise<void> {
  const args = new Set(process.argv.slice(2));
  if (args.has("--help") || args.has("-h")) {
    console.log(USAGE);
    return;
  }

  const doLink = args.has("--link");
  const includeSharedIp = args.has("--shared-ip");
  const strict = args.has("--strict");

  const config = getConfig();
  const authDb = new AuthDb(config.AUTH_DB_URL);

  try {
    let members: MemberLike[] = [];
    try {
      members = await fetchGuildMembers(config.DISCORD_GUILD_ID, config.DISCORD_TOKEN);
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      if (doLink) throw new Error(`Cannot auto-link without the guild member list: ${message}`);
      console.warn(`Skipped Discord member check: ${message}`);
    }

    if (doLink) {
      const { linked, plan } = await autoLinkLegacyAccounts(authDb, members);
      console.log(`Auto-linked ${linked} legacy account(s).`);
      for (const conflict of plan.conflicts) console.log(`  conflict: ${formatLegacyConflict(conflict)}`);
    }

    const [accounts, links] = await Promise.all([authDb.listAccounts(), authDb.listDiscordLinks()]);
    const unlinked = accounts.filter((account) => !links.some((link) => link.accountId === account.id));
    console.log(`Scanned ${accounts.length} accounts (${links.length} linked, ${unlinked.length} unlinked).`);

    const allFlags = buildAccountFlags(accounts, links, members);
    const flags = includeSharedIp ? allFlags : allFlags.filter((flag) => flag.kind !== "shared-ip");

    if (flags.length === 0) {
      console.log("No duplicate or suspicious accounts found.");
      return;
    }

    for (const flag of flags) {
      console.log(`\n[${flag.kind}] ${flag.title}`);
      for (const account of flag.accounts) {
        const discord = account.discordId ?? "unlinked";
        const ip = includeSharedIp ? `  ip=${account.lastIp}` : "";
        console.log(`  #${account.id}  ${account.username.padEnd(24)}  discord=${discord}  gm=${account.gmLevel}${ip}`);
      }
    }

    if (strict) {
      process.exitCode = 1;
    } else {
      console.log("\nFlags are informational; pass --strict to exit non-zero.");
    }
  } finally {
    await authDb.close().catch(() => {});
  }
}

try {
  await main();
} catch (error) {
  const message = error instanceof Error ? error.message : String(error);
  console.error(`flag-accounts failed: ${message}`);
  if (message.includes("ECONNREFUSED")) {
    console.error("Could not reach MySQL. Is the database container running?");
  }
  process.exitCode = 1;
}
