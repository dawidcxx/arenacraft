import "dotenv/config";
import {
  ChannelType,
  Client,
  GatewayIntentBits,
  REST,
  Routes,
  TextChannel,
} from "discord.js";
import mysql from "mysql2/promise";
import { handleRegisterCmd } from "./handleRegisterCmd";
import { Redis } from "ioredis";
import { getHandleAreneaEvents } from "./handleArenaEvents";
import { getConfig } from "./getConfig";
import { requireNotNull } from "./utils";
import { CharacterService } from "./characterService";

const config = getConfig();
console.log("config", config);
const discordBot = new Client({ intents: [GatewayIntentBits.Guilds, GatewayIntentBits.GuildVoiceStates] });
const discordRestClient = new REST().setToken(config.DISCORD_TOKEN);
const authDb = await mysql.createConnection({
  uri: config.ACORE_AUTH_DB,
  namedPlaceholders: true,
});
const charactersDb = await mysql.createConnection({
  uri: config.ACORE_CHARACTERS_DB,
  namedPlaceholders: true,
  rowsAsArray: true,
});
const subscriberRedis = new Redis(config.REDIS_CONNECTION, {
  lazyConnect: true,
});
const fetchRedis = new Redis(config.REDIS_CONNECTION, { lazyConnect: true });

await Promise.all([
  subscriberRedis.connect(),
  fetchRedis.connect(),
  authDb.connect(),
  charactersDb.connect(),
]);

const commands = [
  {
    name: "register",
    description: "Obtian a arenacraft login:password pair",
  },
  {
    name: "ping",
    description: "Check if the bot is alive",
  },
];

await discordRestClient.put(Routes.applicationCommands(config.CLIENT_ID), {
  body: commands,
});

discordBot.on("interactionCreate", async (interaction) => {
  if (!interaction.isCommand()) return;
  const start = +new Date();
  switch (interaction.commandName) {
    case "register": {
      await handleRegisterCmd(authDb, interaction);
      break;
    }
    case "ping": {
      await interaction.user.send(`Ready at your command`);
      break;
    }
  }
  await interaction.reply("Done. Check your DMs");
  console.log(`Handleded slash command in ${+new Date() - start}ms`);
});

discordBot.once("ready", async () => {
  console.log("DiscordBot is ready");
  const guild = discordBot.guilds.cache.first();
  requireNotNull(guild, "No guild found");
  const infoChannel = guild.channels.cache.find(
    (it) =>
      it.name.toUpperCase() === "BOT" && it.type === ChannelType.GuildText,
  ) as TextChannel | undefined;
  requireNotNull(infoChannel, "infoChannel not found");

  const { handleArenaStartedCmd, handleArenaEndedCmd } = getHandleAreneaEvents(
    guild,
    infoChannel,
    new CharacterService(charactersDb, authDb, fetchRedis),
  );

  await subscriberRedis.subscribe("arena-started", "arena-ended");
  subscriberRedis.on("message", async (channel, message) => {
    console.log("Received Redis message", channel, message);
    const started = +new Date();
    switch (channel) {
      case "arena-started": {
        await handleArenaStartedCmd(message);
        break;
      }
      case "arena-ended": {
        await handleArenaEndedCmd(message);
        break;
      }
    }
    console.log(`Handled Redis message in ${+new Date() - started}ms`);
  });
});

await discordBot.login(config.DISCORD_TOKEN);
