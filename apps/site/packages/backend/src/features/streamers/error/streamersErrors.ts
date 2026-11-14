import { HttpStatusCodes } from "@arenacraft/common";
import { HttpError } from "@/lib/HttpErrorMetadata";

@HttpError({ status: HttpStatusCodes.SERVICE_UNAVAILABLE })
export class StreamersUnavailableError extends Error {
  constructor() {
    super("Streamer status is unavailable right now. Try again in a few minutes.");
  }
}
