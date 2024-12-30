import { z } from "zod";

export function getConfig() {
  try {
    return z
      .object({
        DISCORD_TOKEN: z.string(),
        CLIENT_ID: z.string(),
        ACORE_AUTH_DB: z.string(),
        REDIS_CONNECTION: z.string(),
        ACORE_CHARACTERS_DB: z.string(),
      })
      .parse(process.env);
  } catch (e) {
    throw new Error("Failed to get environment variables", { cause: e });
  }
}
