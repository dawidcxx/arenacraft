// Discord handles may contain '.', '_' and digits; the server's account names
// are only guaranteed for letters/digits. We accept the intersection so the
// account name is always usable, and reject everything else as "unsupported".
export const WOW_NAME_PATTERN = /^[A-Za-z0-9]{2,20}$/;

/**
 * Normalizes a Discord username into an ArenaCraft account name.
 * Returns null when the name is not supported.
 */
export function normalizeAccountName(discordUsername: string): string | null {
  const name = discordUsername.trim();
  if (!WOW_NAME_PATTERN.test(name)) return null;
  return name.toUpperCase();
}
