import 'dotenv/config'
import { Client, GatewayIntentBits, REST, Routes } from "discord.js";
import { z } from "zod";
import mysql from "mysql2/promise";
import { handleRegisterCmd } from "./handleRegisterCmd";

const config = getConfig();
console.log('config', config);
const discordBot = new Client({ intents: [GatewayIntentBits.Guilds] });
const discordRestClient = new REST().setToken(config.DISCORD_TOKEN);
const db = await mysql.createConnection({ uri: config.AUTH_DB, namedPlaceholders: true });
await db.connect();

const commands = [
  {
    name: "register",
    description: "Obtian a arenacraft login:password pair",
  },
  {
    name: 'ping',
    description: 'Check if the bot is alive'
  }
];

await discordRestClient.put(Routes.applicationCommands(config.CLIENT_ID), {
  body: commands,
});

discordBot.on("interactionCreate", async (interaction) => {
  if (!interaction.isCommand()) return;
  const start = +new Date();
  switch (interaction.commandName) {
    case "register": {
      await handleRegisterCmd(db, interaction)
      break;
    }
    case 'ping': {
      await interaction.user.send(`Ready at your command`)
    }
  }
  await interaction.reply('Done. Check your DMs')
  console.log(`Handleded slash command in ${(+new Date() - start)}ms`)
});

await discordBot.login(config.DISCORD_TOKEN);

console.log('bot started');

function getConfig() {
  try {
    return z
      .object({
        DISCORD_TOKEN: z.string(),
        CLIENT_ID: z.string(),
        AUTH_DB: z.string(),
      })
      .parse(process.env);
  } catch (e) {
    throw new Error("Failed to get environment variables", { cause: e });
  }
}

