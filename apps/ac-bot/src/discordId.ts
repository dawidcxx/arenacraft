const DISCORD_ID_PATTERN = /^\d{1,20}$/;
const MENTION_PATTERN = /^<@!?(\d+)>$/;

/** Accepts a raw snowflake or a user mention; returns null for anything else. */
export function normalizeDiscordId(value: string): string | null {
  const trimmed = value.trim();
  const mention = trimmed.match(MENTION_PATTERN);
  const id = mention ? (mention[1] ?? "") : trimmed;
  return DISCORD_ID_PATTERN.test(id) ? id : null;
}
