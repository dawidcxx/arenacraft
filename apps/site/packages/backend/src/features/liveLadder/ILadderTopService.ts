import type { LiveLadderMessage } from "@arenacraft/common";
import { InjectionToken } from "@needle-di/core";

export type LadderTopEvent = LiveLadderMessage;
export type LadderTopListener = (event: LadderTopEvent) => void;

export interface ILadderTopService {
  /** Number of live consumers (WebSocket connections). Polling sleeps at zero. */
  readonly subscriberCount: number;

  subscribe(listener: LadderTopListener): () => void;
}

export const LADDER_TOP_SERVICE = new InjectionToken<ILadderTopService>("ILadderTopService");