import { setTimeout } from "node:timers/promises";
import { isAbortError } from "./isAbortError";

export async function sleep(ms: number, signal?: AbortSignal): Promise<void> {
  return setTimeout(ms, undefined, { signal }).catch((error) => {
    if (isAbortError(error, signal)) {
      return;
    }
    throw error;
  });
}