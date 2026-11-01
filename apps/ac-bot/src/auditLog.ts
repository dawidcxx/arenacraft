import { ChannelType } from "discord.js";
import type { Guild, GuildEmoji, TextChannel } from "discord.js";

import { logger } from "./logger";
import type { MatchupEndedEvent, MatchupEvent, Participant } from "./types";

export type EmojiResolver = (classId: number) => string | undefined;

// SharedDefines.h Classes enum -> custom emoji alias uploaded to the guild.
const CLASS_EMOJI_NAMES: Record<number, string> = {
  1: "warrior",
  2: "paladin",
  3: "hunter",
  4: "rogue",
  5: "priest",
  6: "deathknight",
  7: "shaman",
  8: "mage",
  9: "warlock",
  11: "druid",
};

// Map.dbc ids for the WotLK arena maps.
const ARENA_MAP_NAMES: Record<number, string> = {
  559: "Nagrand Arena",
  562: "Blade's Edge Arena",
  572: "Ruins of Lordaeron",
  617: "Dalaran Sewers",
  618: "Ring of Valor",
};

const TEAM_A = "🟦";
const TEAM_B = "🟥";

/** Resolves a class id to the guild's `<:warrior:id>` mention, if uploaded. */
export function buildEmojiResolver(emojis: Iterable<GuildEmoji>): EmojiResolver {
  const byName = new Map<string, GuildEmoji>();
  for (const emoji of emojis) if (emoji.name) byName.set(emoji.name, emoji);

  const resolved = new Map<number, string>();
  for (const [classId, name] of Object.entries(CLASS_EMOJI_NAMES)) {
    const emoji = byName.get(name);
    if (emoji) resolved.set(Number(classId), emoji.toString());
  }
  return (classId) => resolved.get(classId);
}

function playerName(player: Participant, emoji: EmojiResolver): string {
  return `${emoji(player.classId) ?? ""}${player.characterName}`;
}

function signed(delta: number): string {
  return delta >= 0 ? `+${delta}` : String(delta);
}

export function formatDuration(milliseconds: number): string {
  const totalSeconds = Math.max(0, Math.round(milliseconds / 1000));
  const minutes = Math.floor(totalSeconds / 60);
  const seconds = totalSeconds % 60;
  return minutes > 0 ? `${minutes}m${seconds}s` : `${seconds}s`;
}

function teamLine(marker: string, players: Participant[], emoji: EmojiResolver): string {
  return `${marker} ${players.map((player) => `${playerName(player, emoji)} ${player.rating}`).join(" · ")}`;
}

export function formatMatchup(event: MatchupEvent, emoji: EmojiResolver): string {
  const map = ARENA_MAP_NAMES[event.matchup.mapId];
  const header = [`⚔️ SoloQ #${event.instanceId}`, `${event.arenaType}v${event.arenaType}`, map]
    .filter(Boolean)
    .join(" · ");

  return [
    header,
    teamLine(TEAM_A, event.teamA.players, emoji),
    teamLine(TEAM_B, event.teamB.players, emoji),
  ].join("\n");
}

function splitTeams(players: Participant[]): { teamA: Participant[]; teamB: Participant[] } {
  // Team A is the alliance side (teamId 0), team B the horde side (teamId 1);
  // anything unexpected is grouped on side A so no player silently disappears.
  return {
    teamA: players.filter((player) => player.teamId !== 1),
    teamB: players.filter((player) => player.teamId === 1),
  };
}

function resultCell(player: Participant, emoji: EmojiResolver): string {
  const delta = typeof player.ratingDelta === "number" ? ` ${signed(player.ratingDelta)}` : "";
  return `${playerName(player, emoji)}${delta}`;
}

export function formatEnded(event: MatchupEndedEvent, emoji: EmojiResolver): string {
  const { teamA, teamB } = splitTeams(event.players);
  const header = `SoloQ #${event.instanceId}`;

  if (!event.finished) {
    const names = (players: Participant[]) => players.map((player) => player.characterName).join(", ");
    return [`⚠️ ${header} · aborted (invites declined)`, `${TEAM_A} ${names(teamA)}`, `${TEAM_B} ${names(teamB)}`].join(
      "\n",
    );
  }

  const winner = event.winner === "teamB" ? "Team B" : "Team A";
  const duration = formatDuration(event.endedAtMs - event.startedAtMs);
  const line = (marker: string, players: Participant[]) =>
    `${marker} ${players.map((player) => resultCell(player, emoji)).join(" · ")}`;

  return [`🏆 ${header} · ${winner} wins · ${duration}`, line(TEAM_A, teamA), line(TEAM_B, teamB)].join("\n");
}

/**
 * Posts one compact message per solo queue event to the audit log channel.
 * Class icons are resolved from the guild's custom emoji; missing icons simply
 * render as the bare character name.
 */
export class AuditLog {
  private constructor(
    private readonly channel: TextChannel,
    private readonly emoji: EmojiResolver,
  ) {}

  static async create(guild: Guild, channelName: string): Promise<AuditLog | null> {
    const name = channelName.trim();
    if (!name) return null;

    const channels = await guild.channels.fetch();
    const channel = [...channels.values()].find(
      (candidate): candidate is TextChannel =>
        candidate !== null && candidate.type === ChannelType.GuildText && candidate.name.toLowerCase() === name.toLowerCase(),
    );
    if (!channel) {
      logger.warn(`Audit log channel "${channelName}" not found; match messages are disabled`);
      return null;
    }

    await guild.emojis.fetch();
    return new AuditLog(channel, buildEmojiResolver(guild.emojis.cache.values()));
  }

  async matchup(event: MatchupEvent): Promise<void> {
    await this.send(formatMatchup(event, this.emoji));
  }

  async ended(event: MatchupEndedEvent): Promise<void> {
    await this.send(formatEnded(event, this.emoji));
  }

  private async send(content: string): Promise<void> {
    try {
      await this.channel.send({ content, allowedMentions: { parse: [] } });
    } catch (error) {
      logger.warn("Failed to post audit log message", error);
    }
  }
}
