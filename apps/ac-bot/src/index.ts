import { Client, Events, GatewayIntentBits, MessageFlags } from "discord.js";
import type { InteractionReplyOptions } from "discord.js";

import { AuthDb } from "./authDb";
import { CharacterDb } from "./characterDb";
import { getConfig } from "./config";
import { logger } from "./logger";
import { PING_COMMAND, handlePing } from "./pingCommand";
import { startEventSubscriber } from "./redisEvents";
import { handleRegister, REGISTER_COMMAND } from "./registerCommand";
import { handleSetPassword, SET_PASSWORD_COMMAND } from "./setPasswordCommand";
import { handleTopRankers, loadTopRankers, TOP_RANKERS_COMMAND } from "./topRankersCommand";
import { TtlCache } from "./ttlCache";
import { RoomManager } from "./voiceRooms";

const config = getConfig();
const authDb = new AuthDb(config.AUTH_DB_URL);
const characterDb = new CharacterDb(config.CHARACTER_DB_URL);

// Soloq leaderboard queries join several tables; one 30s snapshot per window is
// plenty for a Discord leaderboard and keeps the DB load negligible.
const RANKERS_CACHE_TTL_MS = 30_000;
const rankersCache = new TtlCache(RANKERS_CACHE_TTL_MS, () => loadTopRankers({ authDb, characterDb }));

const client = new Client({
  intents: [GatewayIntentBits.Guilds, GatewayIntentBits.GuildVoiceStates, GatewayIntentBits.GuildMembers],
});

client.once(Events.ClientReady, async (readyClient) => {
  logger.info(`Logged in as ${readyClient.user.tag}`);

  try {
    const guild = await readyClient.guilds.fetch(config.DISCORD_GUILD_ID);
    // Wipe leftover global commands so stale ones do not linger next to the
    // guild commands we actually implement.
    await readyClient.application.commands.set([]);
    await guild.commands.set([REGISTER_COMMAND, SET_PASSWORD_COMMAND, PING_COMMAND, TOP_RANKERS_COMMAND]);
    await guild.members.fetch();

    const rooms = new RoomManager(guild, config);
    await rooms.init();

    const redis = await startEventSubscriber(config.REDIS_URL, rooms);
    const sweep = setInterval(() => void rooms.sweepStale(), 60_000);

    const shutdown = async () => {
      clearInterval(sweep);
      await redis.quit();
      await authDb.close();
      await characterDb.close();
      await readyClient.destroy();
      process.exit(0);
    };
    process.once("SIGINT", () => void shutdown());
    process.once("SIGTERM", () => void shutdown());
  } catch (error) {
    logger.error("Startup failed", error);
    process.exit(1);
  }
});

client.on(Events.InteractionCreate, async (interaction) => {
  if (!interaction.isChatInputCommand()) return;

  try {
    if (interaction.commandName === "register") await handleRegister(interaction, authDb);
    else if (interaction.commandName === "setpassword") await handleSetPassword(interaction, authDb);
    else if (interaction.commandName === "toprankers") await handleTopRankers(interaction, () => rankersCache.get());
    else if (interaction.commandName === "ping") await handlePing(interaction);
  } catch (error) {
    logger.error(`Failed to handle /${interaction.commandName}`, error);
    const reply: InteractionReplyOptions = { content: "Something went wrong, try again later.", flags: MessageFlags.Ephemeral };
    if (interaction.replied || interaction.deferred) await interaction.followUp(reply).catch(() => {});
    else await interaction.reply(reply).catch(() => {});
  }
});

await client.login(config.DISCORD_TOKEN);
