import { sortStreamers, type Streamer } from "@arenacraft/common";
import { inject, injectable } from "@needle-di/core";
import { serializeError } from "serialize-error";
import { Logger } from "@/lib/Logger";
import { SnapshotCache } from "@/shared/SnapshotCache";
import { StreamersUnavailableError } from "./error/streamersErrors";
import { STREAMER_CACHE_TTL_MS } from "./streamers";
import { STREAMER_STATUS_SOURCE, type StreamerStatusSource } from "./StreamerStatusSource";

@injectable()
export class StreamersService {
  private readonly cache: SnapshotCache<Streamer[]>;

  constructor(private readonly source: StreamerStatusSource = inject(STREAMER_STATUS_SOURCE)) {
    this.cache = new SnapshotCache(STREAMER_CACHE_TTL_MS, () => this.load());
  }

  /** The ordered listing: featured live channels, other live channels, then offline channels. */
  list(): Promise<Streamer[]> {
    return this.cache.get();
  }

  private async load(): Promise<Streamer[]> {
    try {
      return sortStreamers(await this.source.statuses());
    } catch (error) {
      logger.error("Failed to load streamer status", { error: serializeError(error) });
      throw new StreamersUnavailableError();
    }
  }
}

const logger = Logger.for(StreamersService);
