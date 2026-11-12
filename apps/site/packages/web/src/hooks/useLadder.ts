import type { LadderEntry } from "@arenacraft/common";
import { useEffect, useState } from "react";
import { ladderApi } from "@/lib/apis";

export type LadderState =
  | { status: "loading" }
  | { status: "error"; message: string }
  | { status: "ready"; entries: LadderEntry[] };

const REFRESH_MS = 60_000;

export function useLadder(): LadderState {
  const [state, setState] = useState<LadderState>({ status: "loading" });

  useEffect(() => {
    let cancelled = false;

    const load = async () => {
      try {
        const entries = await ladderApi.soloq();
        if (!cancelled) setState({ status: "ready", entries });
      } catch (error) {
        if (!cancelled) {
          setState({
            status: "error",
            message: error instanceof Error ? error.message : "Failed to load the ladder",
          });
        }
      }
    };

    void load();
    const id = window.setInterval(() => void load(), REFRESH_MS);

    return () => {
      cancelled = true;
      window.clearInterval(id);
    };
  }, []);

  return state;
}