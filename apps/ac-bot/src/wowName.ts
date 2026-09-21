/**
 * Uses the Discord username (trimmed, lowercase) as the ArenaCraft account
 * name. Players prefer lowercase; the Wow client uppercases account names at
 * login anyway. Storing it lowercase is safe because `account.username` uses a
 * case-insensitive collation, so the auth core's uppercased login still
 * matches. The SRP6 verifier is computed from the uppercased name (see
 * makeRegistrationData), keeping it consistent with what the client sends.
 */
export function normalizeAccountName(discordUsername: string): string {
  return discordUsername.trim().toLowerCase();
}
