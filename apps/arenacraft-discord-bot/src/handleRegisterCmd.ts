import { animals, colors, uniqueNamesGenerator } from "unique-names-generator";
import { AlreadyRegistered } from "./error";
import { randomInt } from "./utils";
import { randomBytes } from "crypto";
import type { CacheType, ChatInputCommandInteraction, MessageContextMenuCommandInteraction, UserContextMenuCommandInteraction } from "discord.js";
import { createUser } from "./createUser";
import type { Connection } from "mysql2/promise";

export async function handleRegisterCmd(db: Connection, interaction: ChatInputCommandInteraction<CacheType> | MessageContextMenuCommandInteraction<CacheType> | UserContextMenuCommandInteraction
) {
    try {
        const randomUsername = uniqueNamesGenerator({
            dictionaries: [colors, animals],
            separator: "",
            length: 2,
        }) + randomInt().toString();
        const randomPassword = randomBytes(6).toString("hex");

        await createUser(db, { username: randomUsername, password: randomPassword, discordId: interaction.user.id })
        await interaction.user.send(`
*Welcome onboard!* Your identity shalll be..

**Username :** \`${randomUsername}\`
**Password  :** \`${randomPassword}\`

Use it when logging into your game client
      `);
        console.log(`Registered "${randomUsername}:${randomPassword}"`)
        return;

    } catch (e) {
        if (e instanceof AlreadyRegistered) {
            await interaction.user.send(`Registration attempt denied: You already haven an account!`);
        } else {
            console.error('Unexpected error while handling register cmd', e);
        }
    }
}