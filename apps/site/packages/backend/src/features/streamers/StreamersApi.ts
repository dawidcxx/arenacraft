import { HttpStatusCodes, streamersContract } from "@arenacraft/common";
import { inject, injectable } from "@needle-di/core";
import { Hono } from "hono";
import { STREAMER_HTTP_CACHE_SECONDS } from "./streamers";
import { StreamersService } from "./StreamersService";

@injectable()
export class StreamersApi {
  constructor(private readonly service: StreamersService = inject(StreamersService)) {}

  build() {
    const router = new Hono();

    router.on(["GET", "HEAD"], streamersContract.list.path, async (c) => {
      const streamers = await this.service.list();
      // The server snapshot is the source of truth; browsers only need a short leash on it.
      c.header("Cache-Control", `public, max-age=${STREAMER_HTTP_CACHE_SECONDS}`);
      return c.json(streamers, HttpStatusCodes.OK);
    });

    return router;
  }
}
