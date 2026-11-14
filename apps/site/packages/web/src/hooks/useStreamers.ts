import { useEffect, useState } from "react";
import { streamersApi } from "@/lib/apis";
import {
  INITIAL_STREAMERS,
  streamerErrorMessage,
  type StreamersState,
} from "@/lib/streamers";

/**
 * Loads the listing once per page view. The backend keeps a one-minute snapshot, so a page reload
 * is what picks up newer Twitch data; there is no polling.
 */
export function useStreamers(): StreamersState {
  const [state, setState] = useState<StreamersState>(INITIAL_STREAMERS);

  useEffect(() => {
    let cancelled = false;

    streamersApi
      .list()
      .then((streamers) => {
        if (!cancelled) setState({ status: "ready", streamers });
      })
      .catch((error: unknown) => {
        if (!cancelled) setState({ status: "error", message: streamerErrorMessage(error) });
      });

    return () => {
      cancelled = true;
    };
  }, []);

  return state;
}
