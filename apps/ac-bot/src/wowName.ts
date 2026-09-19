// Discord handles may contain '.', '_' and digits; letters, digits and
// underscores are usable as ArenaCraft account names. We reject everything else
// as "unsupported".
export const WOW_NAME_PATTERN = /^[A-Za-z0-9_]{2,20}$/;

/**
 * Normalizes a Discord username into an ArenaCraft account name.
 * Returns null when the name is not supported.
 */
export function normalizeAccountName(discordUsername: string): string | null {
  const name = discordUsername.trim();
  if (!WOW_NAME_PATTERN.test(name)) return null;
  return name.toUpperCase();
}
