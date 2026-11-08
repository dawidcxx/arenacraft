import { HttpStatusCodes, ladderContract } from "@arenacraft/common";
import { inject, injectable } from "@needle-di/core";
import { Hono } from "hono";
import { CONFIG, type Config } from "@/shared/Config";
import { LadderService } from "./LadderService";

@injectable()
export class LadderApi {
  constructor(
    private readonly ladderService: LadderService = inject(LadderService),
    private readonly config: Config = inject(CONFIG),
  ) {}

  build() {
    const router = new Hono();

    router.on(["GET", "HEAD"], ladderContract.soloq.path, async (c) => {
      const entries = await this.ladderService.getSoloq();
      c.header("Cache-Control", `private, max-age=${this.config.cacheTtlSeconds}`);
      return c.json(entries, HttpStatusCodes.OK);
    });

    return router;
  }
}