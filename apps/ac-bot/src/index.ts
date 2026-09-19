import { Client, Events, GatewayIntentBits, MessageFlags } from "discord.js";
import type { InteractionReplyOptions } from "discord.js";

import { AuthDb } from "./authDb";
import { getConfig } from "./config";
import { logger } from "./logger";
import { startEventSubscriber } from "./redisEvents";
import { handleRegister, REGISTER_COMMAND } from "./registerCommand";
import { RoomManager } from "./voiceRooms";

const config = getConfig();
const authDb = new AuthDb(config.AUTH_DB_URL);

const client = new Client({
  intents: [GatewayIntentBits.Guilds, GatewayIntentBits.GuildVoiceStates, GatewayIntentBits.GuildMembers],
});

client.once(Events.ClientReady, async (readyClient) => {
  logger.info(`Logged in as ${readyClient.user.tag}`);

  try {
    const guild = await readyClient.guilds.fetch(config.DISCORD_GUILD_ID);
    await guild.commands.set([REGISTER_COMMAND]);
    await guild.members.fetch();

    const rooms = new RoomManager(guild, config);
    await rooms.init();

    const redis = await startEventSubscriber(config.REDIS_URL, rooms);
    const sweep = setInterval(() => void rooms.sweepStale(), 60_000);

    const shutdown = async () => {
      clearInterval(sweep);
      await redis.quit();
      await authDb.close();
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
  if (!interaction.isChatInputCommand() || interaction.commandName !== "register") return;

  try {
    await handleRegister(interaction, authDb);
  } catch (error) {
    logger.error("Failed to handle /register", error);
    const reply: InteractionReplyOptions = { content: "Something went wrong, try again later.", flags: MessageFlags.Ephemeral };
    if (interaction.replied || interaction.deferred) await interaction.followUp(reply).catch(() => {});
    else await interaction.reply(reply).catch(() => {});
  }
});

await client.login(config.DISCORD_TOKEN);
