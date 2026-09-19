/**
 * Uses the Discord username verbatim as the ArenaCraft account name. The auth
 * core uppercases account names, so we uppercase here to keep the SRP6 verifier
 * consistent with what the client sends at login.
 */
export function normalizeAccountName(discordUsername: string): string {
  return discordUsername.trim().toUpperCase();
}
