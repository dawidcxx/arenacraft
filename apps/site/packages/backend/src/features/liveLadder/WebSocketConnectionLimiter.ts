import { injectable } from "@needle-di/core";

/** Hard cap on concurrent live ladder WebSockets; excess upgrades are rejected with 503. */
export const LIVE_LADDER_MAX_CONNECTIONS = 500;

@injectable()
export class WebSocketConnectionLimiter {
  private active = 0;

  get count(): number {
    return this.active;
  }

  /**
   * Reserves a connection slot. Returns a release function, or `null` when the limit is reached.
   * The release function is idempotent so both `onClose` and `onError` can call it safely.
   */
  tryAcquire(): (() => void) | null {
    if (this.active >= LIVE_LADDER_MAX_CONNECTIONS) return null;
    this.active++;

    let released = false;
    return () => {
      if (released) return;
      released = true;
      this.active--;
    };
  }
}