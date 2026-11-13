import { HttpStatusCodes, PlayerOverlayParams, playerOverlayContract } from "@arenacraft/common";
import { inject, injectable } from "@needle-di/core";
import { Hono } from "hono";
import * as v from "valibot";
import { PlayerOverlayNotFoundError } from "./error/playerOverlayErrors";
import { PlayerOverlayService } from "./PlayerOverlayService";

@injectable()
export class PlayerOverlayApi {
  constructor(private readonly service: PlayerOverlayService = inject(PlayerOverlayService)) {}

  build() {
    const router = new Hono();

    router.on(["GET", "HEAD"], playerOverlayContract.get.path, async (c) => {
      const params = v.safeParse(PlayerOverlayParams, { token: c.req.param("token") });
      if (!params.success) throw new PlayerOverlayNotFoundError();

      const overlay = await this.service.get(params.output.token);
      // The overlay polls on its own schedule; the token is a secret, so keep it out of caches.
      c.header("Cache-Control", "no-store");
      return c.json(overlay, HttpStatusCodes.OK);
    });

    return router;
  }
}