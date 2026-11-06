import { basicAuth } from "hono/basic-auth";
import type { MiddlewareHandler } from "hono";
import { createHash, timingSafeEqual } from "node:crypto";

// Compare digests with a constant-time equality check, so token matching cannot leak the secret
// through response timing.
function tokenMatches(candidate: string, tokens: string[]): boolean {
  const candidateDigest = createHash("sha256").update(candidate).digest();
  let matched = false;
  for (const token of tokens) {
    const tokenDigest = createHash("sha256").update(token).digest();
    if (timingSafeEqual(candidateDigest, tokenDigest)) matched = true;
  }
  return matched;
}

// HTTP Basic auth: any configured token is accepted as either the username or the password, so both
// `user:<token>` and `<token>:` work.
export function basicTokenAuth(tokens: string[]): MiddlewareHandler {
  return basicAuth({
    realm: "arenacraft",
    verifyUser: (username, password) => tokenMatches(username, tokens) || tokenMatches(password, tokens),
    invalidUserMessage: { error: "unauthorized" },
  });
}
