import mysql from "mysql2/promise";
import { UnableToFindCharacter } from "./error";
import { z } from "zod";
import type Redis from "ioredis";

export class CharacterService {
  constructor(
    private readonly charactersDb: mysql.Connection,
    private readonly authDb: mysql.Connection,
    private readonly redis: Redis,
  ) {}

  async getDiscordIdByCharacterName(characterName: string): Promise<string> {
    const cachedDiscordId = await this.redis.get(
      `CHARACTERS_TO_DISCORD_ID_LOOKUP.${characterName}`,
    );
    if (cachedDiscordId) {
      return cachedDiscordId;
    }
    const discordId =
      await this.getDiscordIdByCharacterNameFromDb(characterName);
    await this.redis.set(
      `CHARACTERS_TO_DISCORD_ID_LOOKUP.${characterName}`,
      discordId,
    );
    return discordId;
  }

  async getDiscordIdByCharacterNameFromDb(
    characterName: string,
  ): Promise<string> {
    const [characterRowsRaw] = await this.charactersDb.query(
      `SELECT account from characters where name=?`,
      [characterName],
    );
    if (!Array.isArray(characterRowsRaw) || characterRowsRaw.length === 0) {
      throw new UnableToFindCharacter(characterName);
    }
    const accountId = SelectCharactersResultSchema.parse(
      characterRowsRaw[0],
    )[0];
    const [discordIdRowsRaw] = await this.authDb.query(
      `SELECT email from account where id=?`,
      [accountId],
    );
    if (!Array.isArray(discordIdRowsRaw) || discordIdRowsRaw.length === 0) {
      throw new UnableToFindCharacter(characterName);
    }
    const { email } = SelectDiscordIdResultSchema.parse(discordIdRowsRaw)[0];
    return email;
  }
}

const SelectCharactersResultSchema = z.array(z.number());
const SelectDiscordIdResultSchema = z.array(z.object({ email: z.string() }));
