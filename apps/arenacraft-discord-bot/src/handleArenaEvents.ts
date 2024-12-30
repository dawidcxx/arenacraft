import { ChannelType, Collection, TextChannel, VoiceChannel, type Guild } from "discord.js";
import { assert, isNil, requireNotNull, throttle } from "./utils";
import { z } from "zod";
import { AllRoomsTakenError } from "./error";
import type { CharacterService } from "./characterService";

export function getHandleAreneaEvents(
  guild: Guild,
  infoChannel: TextChannel,
  characterService: CharacterService,
) {
  const voiceChannels = guild.channels.cache.filter(
    (channel) => channel.type === ChannelType.GuildVoice,
  );
  const waitingRoom = voiceChannels.find(
    (channel) => channel.name === "Waiting Room",
  );
  requireNotNull(waitingRoom, "No waiting room found");

  const gameChannels = voiceChannels.filter((channel) =>
    channel.name.startsWith("game-"),
  );

  const rooms = new Rooms(
    buildRooms(waitingRoom, gameChannels),
    waitingRoom,
    characterService,
  );

  async function handleArenaStartedCmd(messageRaw: string) {
    const message = ArenaStartedEventSchema.safeParse(JSON.parse(messageRaw));
    if (message.success === false) {
      console.error("Failed to parse message", message.error);
      return;
    }
    try { 
    await rooms.book(message.data);
    } catch (e) {
      if (e instanceof AllRoomsTakenError) {
        await infoChannel.send(
          `Ran out of public rooms to allocate voice channel for game: { id =  ${e.instanceId} }`,
        );
      } else {
        throw e;
      }
    }
  };

  async function handleArenaEndedCmd(messageRaw: string) {
    const message = ArenaEndedEventSchema.safeParse(JSON.parse(messageRaw));

    if (message.success === false) {
      console.error("Failed to parse message", message.error);
      return;
    }

    await rooms.empty(message.data);
  }

  return { handleArenaStartedCmd, handleArenaEndedCmd };
}

const ArenaStartedEventSchema = z.object({
  instanceId: z.number(),
  arenaType: z.number(),
  team1: z.array(
    z.object({
      characterName: z.string(),
      classId: z.number(),
    }),
  ),
  team2: z.array(
    z.object({
      characterName: z.string(),
      classId: z.number(),
    }),
  ),
});

type ArenaStartedEvent = z.infer<typeof ArenaStartedEventSchema>;

const ArenaEndedEventSchema = z.object({
  instanceId: z.number(),
});

type ArenaEndedEvent = z.infer<typeof ArenaEndedEventSchema>;

function buildRooms(waitingRoom: VoiceChannel, gameChannels: Collection<string, VoiceChannel>) {
  const groupping: Record<string, Array<VoiceChannel>> = {};
  const rooms: Array<Room> = [];
  // remap channels from { [channel1-1]: .., [channel1-2] } to { "1": [channel1, channel2] } etc
  for (const channel of gameChannels.values()) {
    // example: game-1-1
    const channelGroupId = channel.name.split("-")[1];
    requireNotNull(channelGroupId, "Failed to obtain channel group id");
    if (groupping[channelGroupId]) {
      groupping[channelGroupId].push(channel);
    } else {
      groupping[channelGroupId] = [channel];
    }
  }
  for (const channelPair of Object.values(groupping)) {
    assert(channelPair.length === 2, `Invalid channel group size: ${channelPair.length}`);
    rooms.push(new Room(waitingRoom, channelPair[0], channelPair[1]));
  }

  return rooms;
}

class Room {
  public instanceId: number | null = null;

  constructor(
    public waitingRoom: VoiceChannel,
    public channel1: VoiceChannel,
    public channel2: VoiceChannel,
  ) { }

  public get isTaken() {
    return this.instanceId !== null;
  }

  async take(
    event: ArenaStartedEvent,
    characterService: CharacterService, 
  ) {
    this.instanceId = event.instanceId;
    
    const team1DiscordIds = await Promise.all(
      event.team1.map((it) =>
        characterService.getDiscordIdByCharacterName(it.characterName),
      ),
    );

    const team2DiscordIds = await Promise.all(
      event.team2.map((it) =>
        characterService.getDiscordIdByCharacterName(it.characterName),
      ),
    );

    // queues get processed once every 5s 
    await throttle('REFRESH_WAITING_ROOMS', async () => {
       await this.waitingRoom.fetch();
    });

    let moveCount = 0;
    const moveToGameChannel = (channel: VoiceChannel) => async (discordId: string) => {
      const member = this.waitingRoom.members.get(discordId);
      if (isNil(member)) {
        return;
      }
      moveCount += 1;
      await member.voice.setChannel(channel);
    }

    const team1ChannelMoves = team1DiscordIds.map(moveToGameChannel(this.channel1));
    const team2ChannelMoves = team2DiscordIds.map(moveToGameChannel(this.channel2));

    await Promise.all([team1ChannelMoves, team2ChannelMoves].flat());

    console.info(`Moved ${moveCount} members to game channels for game '${event.instanceId}'`);

  }

  async release() {
    this.instanceId = null;

    const members1 = this.channel1.members;
    const members2 = this.channel2.members;
    
    const channelMoves = [...members1.values(), ...members2.values()].map(async (member) => {  
      await member.voice.setChannel(this.waitingRoom);
    });

    await Promise.all(channelMoves);
  }
}

class Rooms {
  constructor(
    public readonly rooms: Room[],
    public readonly waitRoomChannel: VoiceChannel,
    public readonly characterService: CharacterService,
  ) { }

  getRoomByInstanceId(instanceId: number): Room | null {
    const room = this.rooms.find((it) => it.instanceId);
    return room ?? null;
  }

  async book(event: ArenaStartedEvent) {
    const firstEmptyRoom = this.rooms.find((it) => !it.isTaken);
    if (isNil(firstEmptyRoom)) {
      throw new AllRoomsTakenError(event.instanceId);
    }
    await firstEmptyRoom.take(event, this.characterService);
  }

  async empty(event: ArenaEndedEvent) {
    const room = this.getRoomByInstanceId(event.instanceId);
    if (isNil(room)) {
      return;
    }
    await room.release();
  }
}
