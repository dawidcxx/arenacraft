import { ChannelType, OverwriteType } from "discord.js";
import type { Guild, GuildMember, TextChannel, VoiceChannel } from "discord.js";

import type { Config } from "./config";
import { logger } from "./logger";

const MEMBER_REFRESH_COOLDOWN_MS = 5 * 60_000;

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
}

/**
 * Allocates players into paired voice channels per matchup and releases them
 * back to the waiting room. Occupancy is best-effort: rooms holding players are
 * treated as taken even when the bot has no instance mapping (after a crash),
 * and rooms held longer than STALE_ROOM_MINUTES are reclaimed by sweepStale().
 */
export class RoomManager {
  private rooms: Room[] = [];
  private waitingRoom: VoiceChannel | null = null;
  private infoChannel: TextChannel | null = null;
  private memberRefresh: Promise<void> | null = null;
  private lastMemberRefresh = 0;

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
    }));

    await Promise.all(this.rooms.map((room) => this.hideRoom(room)));

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
      this.admitAndMove(teamADiscordIds, room.first),
      this.admitAndMove(teamBDiscordIds, room.second),
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

  // Anti-snipe: game rooms are hidden from everyone; book() lets the players of a match see and join their team's
  // room (a per-member overwrite) and freeRoom() takes it back. Hiding is restored on every startup, and leftover
  // per-member grants of empty rooms (bot crash mid-match) are dropped. The bot keeps access to move people.
  private async hideRoom(room: Room): Promise<void> {
    for (const channel of [room.first, room.second]) {
      try {
        await channel.permissionOverwrites.edit(this.guild.roles.everyone, { ViewChannel: false });
        const me = this.guild.members.me;
        if (me) await channel.permissionOverwrites.edit(me, { ViewChannel: true, Connect: true, MoveMembers: true });
        if (channel.members.size === 0) await this.revokeMembers(channel);
      } catch (error) {
        logger.warn(`Failed to hide game room "${channel.name}"`, error);
      }
    }
  }

  private async revokeMembers(channel: VoiceChannel): Promise<void> {
    const me = this.guild.members.me?.id;
    const grants = channel.permissionOverwrites.cache.filter(
      (overwrite) => overwrite.type === OverwriteType.Member && overwrite.id !== me,
    );
    await Promise.all(
      grants.map((overwrite) =>
        overwrite.delete("Match over").catch((error) => logger.warn(`Failed to revoke ${overwrite.id} on ${channel.name}`, error)),
      ),
    );
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
    const members = [...room.first.members.values(), ...room.second.members.values()];
    const moves = await Promise.all(members.map((member) => this.moveMember(member, this.waitingRoom)));
    const moved = moves.filter(Boolean).length;
    await Promise.all([this.revokeMembers(room.first), this.revokeMembers(room.second)]);
    room.instanceId = null;
    room.bookedAt = null;
    logger.info(`Released ${roomName(room)} (moved ${moved} players back)`);
  }

  // Lets each player of the team see and join the (hidden) room, then moves the ones already in voice. Players
  // not in voice keep the access and can join by themselves. Discord has no bulk voice-move endpoint; each move
  // is its own request, so we issue them all concurrently (a match is at most 6) instead of one at a time.
  private async admitAndMove(discordIds: string[], channel: VoiceChannel): Promise<number> {
    const moves = await Promise.all(
      discordIds.map(async (discordId) => {
        const member = await this.resolveMember(discordId);
        if (!member) {
          logger.debug(`No Discord member for id ${discordId}`);
          return false;
        }
        try {
          await channel.permissionOverwrites.edit(member, { ViewChannel: true, Connect: true }, { reason: "Soloq match" });
        } catch (error) {
          logger.warn(`Failed to admit ${member.user.username} to ${channel.name}`, error);
        }
        return this.moveMember(member, channel);
      }),
    );
    return moves.filter(Boolean).length;
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

  private async moveMember(member: GuildMember, channel: VoiceChannel | null): Promise<boolean> {
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
