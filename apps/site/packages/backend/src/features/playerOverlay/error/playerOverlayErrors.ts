import { HttpStatusCodes } from "@arenacraft/common";
import { HttpError } from "@/lib/HttpErrorMetadata";

@HttpError({ status: HttpStatusCodes.NOT_FOUND })
export class PlayerOverlayNotFoundError extends Error {
  constructor() {
    super("Unknown overlay link. Run /overlay in Discord to get a new one.");
  }
}