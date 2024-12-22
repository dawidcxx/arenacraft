import { Client, GatewayIntentBits, REST, Routes } from "discord.js";
import { z } from "zod";
import {
  uniqueNamesGenerator,
  adjectives,
  colors,
  animals,
} from "unique-names-generator";
import { randomBytes } from "crypto";
import mysql from "mysql2/promise";

const config = getConfig();
const discordBot = new Client({ intents: [GatewayIntentBits.Guilds] });
const discordRestClient = new REST().setToken(config.DISCORD_TOKEN);
const db = await mysql.createConnection(config.AUTH_DB);
await db.connect();

const commands = [
  {
    name: "register",
    description: "Obtian a arenacraft login:password pair",
  },
];

await discordRestClient.put(Routes.applicationCommands(config.CLIENT_ID), {
  body: commands,
});

discordBot.on("interactionCreate", async (interaction) => {
  if (!interaction.isCommand()) return;

  switch (interaction.commandName) {
    case "register": {
      const randomUsername = uniqueNamesGenerator({
        dictionaries: [adjectives, colors, animals],
        separator: "",
        length: 3,
      });
      const randomPassword = randomBytes(16).toString("hex");
      interaction.user.send(`
*Welcome onboard!* Your identity shalll be..

**Username :** \`${randomUsername}\`
**Password  :** \`${randomPassword}\`

Use it when logging into your game client
      `);
      interaction.reply("Account registered");
      return;
    }
  }
});

await discordBot.login(config.DISCORD_TOKEN);

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

