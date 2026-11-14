import { ChannelType } from "discord.js";
import type { Guild, GuildMember, TextChannel, VoiceBasedChannel, VoiceChannel } from "discord.js";

import type { Config } from "./config";
import { logger } from "./logger";

const MEMBER_REFRESH_COOLDOWN_MS = 5 * 60_000;

// Lounge channels players may be returned to after a match, besides the waiting
// room. Anything else (a game room, a private channel, ...) releases to the
// waiting room instead.
const EXTRA_RETURN_ROOMS = ["Chill Room", "Secret Room"];

export interface ChannelLike {
  id: string;
  name: string;
}

export interface RoomGroup {
  group: number;
  first: ChannelLike;
  second: ChannelLike;
}

/**
 * Groups "game-<n>-<k>" voice channels into rooms of two, keyed by <n> and
 * ordered by <k>. Groups without exactly two channels are ignored.
 */
export function groupGameRooms(channels: ChannelLike[], pattern: RegExp): RoomGroup[] {
  const byGroup = new Map<number, { suffix: number; channel: ChannelLike }[]>();

  for (const channel of channels) {
    const match = pattern.exec(channel.name);
    if (!match) continue;
    const group = Number(match[1]);
    const suffix = Number(match[2]);
    if (!Number.isFinite(group) || !Number.isFinite(suffix)) continue;

    const entries = byGroup.get(group) ?? [];
    entries.push({ suffix, channel });
    byGroup.set(group, entries);
  }

  const rooms: RoomGroup[] = [];
  for (const [group, entries] of [...byGroup.entries()].sort((a, b) => a[0] - b[0])) {
    if (entries.length !== 2) continue;
    entries.sort((a, b) => a.suffix - b.suffix);
    rooms.push({ group, first: entries[0]!.channel, second: entries[1]!.channel });
  }
  return rooms;
}

interface Room {
  first: VoiceChannel;
  second: VoiceChannel;
  instanceId: number | null;
  bookedAt: number | null;
  /** Member id -> return channel id, captured when the match was booked. */
  origins: Map<string, string>;
  /** True while a release is moving members out, so a sweep cannot double-free. */
  releasing: boolean;
}

/** Case-insensitive match against a list of channel names. */
export function matchesChannelName(channelName: string, names: readonly string[]): boolean {
  return names.some((name) => sameName(channelName, name));
}

/**
 * Picks where a released player lands: their recorded origin when it is still a
 * configured return channel, the waiting room otherwise.
 */
export function selectReturnChannel<T extends ChannelLike>(
  originId: string | undefined,
  returnChannels: ReadonlyMap<string, T>,
  waitingRoom: T | null,
): T | null {
  if (originId) {
    const origin = returnChannels.get(originId);
    if (origin) return origin;
  }
  return waitingRoom;
}

/**
 * Allocates players into paired voice channels per matchup and releases them
 * back to the channel they waited in. Only the waiting room and EXTRA_RETURN_ROOMS
 * are restored; anything else (another game room, a private channel, ...) falls
 * back to the waiting room so a desync cannot drop players into a room a later
 * match owns. Occupancy is best-effort: rooms holding players are treated as
 * taken even when the bot has no instance mapping (after a crash), and rooms
 * held longer than STALE_ROOM_MINUTES are reclaimed by sweepStale().
 */
export class RoomManager {
  private rooms: Room[] = [];
  private waitingRoom: VoiceChannel | null = null;
  private infoChannel: TextChannel | null = null;
  private memberRefresh: Promise<void> | null = null;
  private lastMemberRefresh = 0;
  private returnChannels = new Map<string, VoiceChannel>();

  constructor(
    private readonly guild: Guild,
    private readonly config: Config,
  ) {}

  async init(): Promise<void> {
    const channels = await this.guild.channels.fetch();
    const voiceChannels = [...channels.values()].filter(
      (channel): channel is VoiceChannel => channel !== null && channel.type === ChannelType.GuildVoice,
    );

    this.waitingRoom = voiceChannels.find((channel) => sameName(channel.name, this.config.WAITING_ROOM_NAME)) ?? null;
    if (!this.waitingRoom)
      logger.warn(`Waiting room voice channel "${this.config.WAITING_ROOM_NAME}" not found`);
    else await this.muteWaitingRoom();

    // Only the waiting room and the lounge rooms above are worth restoring
    // after a match. Anything else is treated as incidental (e.g. someone
    // waiting in a game room) and releases to the waiting room instead.
    // Resolved once at startup like the rest of the voice layout.
    this.returnChannels = new Map(
      voiceChannels
        .filter(
          (channel) => channel.id === this.waitingRoom?.id || matchesChannelName(channel.name, EXTRA_RETURN_ROOMS),
        )
        .map((channel) => [channel.id, channel]),
    );

    this.infoChannel =
      [...channels.values()].find(
        (channel): channel is TextChannel =>
          channel !== null && channel.type === ChannelType.GuildText && sameName(channel.name, this.config.INFO_CHANNEL_NAME),
      ) ?? null;

    const pattern = new RegExp(this.config.GAME_CHANNEL_PATTERN);
    const groups = groupGameRooms(
      voiceChannels.map((channel) => ({ id: channel.id, name: channel.name })),
      pattern,
    );
    const byId = new Map(voiceChannels.map((channel) => [channel.id, channel]));
    this.rooms = groups.map((group) => ({
      first: byId.get(group.first.id)!,
      second: byId.get(group.second.id)!,
      instanceId: null,
      bookedAt: null,
      origins: new Map(),
      releasing: false,
    }));

    const now = Date.now();
    for (const room of this.rooms) {
      if (memberCount(room) > 0) {
        // Crash recovery: members are present but the instance mapping is gone.
        room.bookedAt = now;
        logger.warn(`Room ${roomName(room)} already has members; treating as occupied`);
      }
    }

    logger.info(`Discovered ${this.rooms.length} game room(s) in ${this.guild.name}`);
  }

  async book(instanceId: number, teamADiscordIds: string[], teamBDiscordIds: string[]): Promise<void> {
    if (this.rooms.some((candidate) => candidate.instanceId === instanceId)) {
      logger.debug(`Instance ${instanceId} is already booked; ignoring duplicate event`);
      return;
    }

    const room = this.rooms.find((candidate) => this.isFree(candidate));
    if (!room) {
      await this.notify(`No free game rooms for matchup \`${instanceId}\`.`);
      logger.warn(`No free game room for instance ${instanceId}`);
      return;
    }

    room.instanceId = instanceId;
    room.bookedAt = Date.now();

    const [movedA, movedB] = await Promise.all([
      this.moveDiscordIds(teamADiscordIds, room.first, room),
      this.moveDiscordIds(teamBDiscordIds, room.second, room),
    ]);
    logger.info(`Booked ${roomName(room)} for instance ${instanceId} (moved ${movedA + movedB} players)`);
  }

  async release(instanceId: number, discordIds: string[] = []): Promise<void> {
    let room = this.rooms.find((candidate) => candidate.instanceId === instanceId);
    if (!room) {
      room = this.findRoomByDiscordIds(discordIds);
      if (room) logger.warn(`Recovered ${roomName(room)} for unknown instance ${instanceId}`);
    }
    if (!room) {
      logger.debug(`Ended event for instance ${instanceId} has no associated room`);
      return;
    }
    await this.freeRoom(room);
  }

  // Deny Speak on the channel itself instead of server-muting each member. A
  // per-member mute can outlive the bot (crash, restart, manual channel moves)
  // and leave players stuck muted; a channel overwrite only applies inside the
  // waiting room and is restored on every startup.
  private async muteWaitingRoom(): Promise<void> {
    if (!this.waitingRoom) return;
    try {
      await this.waitingRoom.permissionOverwrites.edit(this.guild.roles.everyone, { Speak: false });
      logger.info(`Muted waiting room "${this.waitingRoom.name}"`);
    } catch (error) {
      logger.warn(`Failed to mute waiting room "${this.waitingRoom.name}"`, error);
    }
  }

  async sweepStale(): Promise<void> {
    const maxAgeMs = this.config.STALE_ROOM_MINUTES * 60_000;
    const now = Date.now();
    for (const room of this.rooms) {
      if (room.bookedAt !== null && now - room.bookedAt >= maxAgeMs) {
        logger.warn(`Room ${roomName(room)} held longer than ${this.config.STALE_ROOM_MINUTES}m; releasing`);
        await this.freeRoom(room);
      }
    }
  }

  private isFree(room: Room): boolean {
    return room.bookedAt === null && memberCount(room) === 0;
  }

  private findRoomByDiscordIds(discordIds: string[]): Room | undefined {
    if (discordIds.length === 0) return undefined;
    const wanted = new Set(discordIds);
    return this.rooms.find((room) =>
      [room.first, room.second].some((channel) =>
        [...channel.members.values()].some((member) => wanted.has(member.id)),
      ),
    );
  }

  private async freeRoom(room: Room): Promise<void> {
    // release() and sweepStale() can both reach the same room (a late ended
    // event, a stale reclaim). A second pass would find the origins already
    // consumed and drag everyone to the waiting room, so let the first finish.
    if (room.releasing) {
      logger.debug(`Room ${roomName(room)} is already being released`);
      return;
    }
    room.releasing = true;
    try {
      const members = [...room.first.members.values(), ...room.second.members.values()];
      let moved = 0;
      await Promise.all(
        members.map(async (member) => {
          if (await this.returnMember(room, member)) moved++;
        }),
      );
      room.instanceId = null;
      // If a move failed (deleted origin, missing Move Members, ...) the player
      // is still in the room: keep it marked as held so sweepStale retries
      // instead of leaving it outside the free pool forever.
      room.bookedAt = memberCount(room) > 0 ? Date.now() : null;
      logger.info(
        `Released ${roomName(room)} (moved ${moved} players back` +
          (room.bookedAt ? ", some members still inside; will retry)" : ")"),
      );
    } finally {
      // Covers players who left voice mid-match too; a per-room map emptied on
      // release cannot grow over the bot's uptime.
      room.origins.clear();
      room.releasing = false;
    }
  }

  private async returnMember(room: Room, member: GuildMember): Promise<boolean> {
    const target = selectReturnChannel(room.origins.get(member.id), this.returnChannels, this.waitingRoom);
    if (!target) return false;
    if (await this.moveMember(member, target)) return true;
    // Never leave a player stuck in a game room because their origin vanished.
    if (target.id === this.waitingRoom?.id) return false;
    return this.moveMember(member, this.waitingRoom);
  }

  // Discord has no bulk voice-move endpoint; each move is its own request, so we
  // issue them all concurrently (a match is at most 6) instead of one at a time.
  private async moveDiscordIds(discordIds: string[], channel: VoiceChannel, room: Room): Promise<number> {
    const moves = await Promise.all(
      discordIds.map(async (discordId) => {
        const member = await this.resolveMember(discordId);
        if (!member) {
          logger.debug(`No Discord member for id ${discordId}`);
          return false;
        }
        this.recordOrigin(room, member);
        return this.moveMember(member, channel);
      }),
    );
    return moves.filter(Boolean).length;
  }

  /**
   * Remembers where to put a player back. Only lobby-ish channels qualify; a
   * player picked up from a game room or some other channel is sent to the
   * waiting room instead (recording it could return them into a room a later
   * match has since booked).
   */
  private recordOrigin(room: Room, member: GuildMember): void {
    const current = member.voice.channel;
    if (current && this.returnChannels.has(current.id)) {
      room.origins.set(member.id, current.id);
    } else {
      room.origins.delete(member.id);
    }
  }

  private async resolveMember(discordId: string): Promise<GuildMember | undefined> {
    let member = this.guild.members.cache.get(discordId);
    if (!member) {
      await this.refreshMembers();
      member = this.guild.members.cache.get(discordId);
    }
    return member;
  }

  // A cache miss should not turn every unknown account into a full member
  // fetch. The cache is populated on startup and kept fresh by the GuildMembers
  // intent, so we only re-fetch when it is empty, and at most once per cooldown.
  private async refreshMembers(): Promise<void> {
    if (this.memberRefresh) return this.memberRefresh;
    if (this.guild.members.cache.size > 0) return;
    if (Date.now() - this.lastMemberRefresh < MEMBER_REFRESH_COOLDOWN_MS) return;

    this.lastMemberRefresh = Date.now();
    this.memberRefresh = this.guild.members
      .fetch()
      .then(() => undefined)
      .catch((error) => {
        logger.warn("Failed to refresh member cache", error);
      })
      .finally(() => {
        this.memberRefresh = null;
      });
    return this.memberRefresh;
  }

  private async moveMember(member: GuildMember, channel: VoiceBasedChannel | null): Promise<boolean> {
    if (!channel) return false;
    // Players without voice simply stay out of the rooms.
    if (!member.voice.channel) return false;
    if (member.voice.channelId === channel.id) return true;

    try {
      await member.voice.setChannel(channel);
      return true;
    } catch (error) {
      logger.warn(`Failed to move ${member.user.username} into ${channel.name}`, error);
      return false;
    }
  }

  private async notify(message: string): Promise<void> {
    if (!this.infoChannel) return;
    try {
      await this.infoChannel.send(message);
    } catch (error) {
      logger.warn("Failed to send info channel message", error);
    }
  }
}

function sameName(a: string, b: string): boolean {
  return a.toLowerCase() === b.toLowerCase();
}

function memberCount(room: Room): number {
  return room.first.members.size + room.second.members.size;
}

function roomName(room: Room): string {
  return `${room.first.name}/${room.second.name}`;
}
