import { AuthDb } from "../src/authDb";
import { getConfig } from "../src/config";
import { buildAccountFlags } from "../src/duplicateAccounts";
import type { MemberLike } from "../src/duplicateAccounts";

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
  const config = getConfig();
  const authDb = new AuthDb(config.AUTH_DB_URL);

  try {
    const [accounts, links] = await Promise.all([authDb.listAccounts(), authDb.listDiscordLinks()]);

    let members: MemberLike[] = [];
    try {
      members = await fetchGuildMembers(config.DISCORD_GUILD_ID, config.DISCORD_TOKEN);
    } catch (error) {
      console.warn(`Skipped Discord member check: ${error instanceof Error ? error.message : String(error)}`);
    }

    const unlinked = accounts.filter((account) => !links.some((link) => link.accountId === account.id));
    console.log(`Scanned ${accounts.length} accounts (${links.length} linked, ${unlinked.length} unlinked).`);

    const flags = buildAccountFlags(accounts, links, members);
    if (flags.length === 0) {
      console.log("No duplicate or suspicious accounts found.");
      return;
    }

    for (const flag of flags) {
      console.log(`\n[${flag.kind}] ${flag.title}`);
      for (const account of flag.accounts) {
        const discord = account.discordId ?? "unlinked";
        console.log(`  #${account.id}  ${account.username.padEnd(24)}  discord=${discord}  ip=${account.lastIp}`);
      }
    }

    process.exitCode = 1;
  } finally {
    await authDb.close();
  }
}

await main();
