import { PLAYER_OVERLAY_TOKEN_PATTERN } from "@arenacraft/common";
import { useEffect, useState } from "react";
import { playerOverlayApi } from "@/lib/apis";
import {
  INITIAL_PLAYER_OVERLAY,
  isNotFoundError,
  PLAYER_OVERLAY_POLL_MS,
  type PlayerOverlayView,
} from "@/lib/playerOverlay";

export function usePlayerOverlay(token: string): PlayerOverlayView {
  const [view, setView] = useState<PlayerOverlayView>(INITIAL_PLAYER_OVERLAY);

  useEffect(() => {
    if (!PLAYER_OVERLAY_TOKEN_PATTERN.test(token)) {
      setView({ status: "invalid", overlay: null });
      return;
    }

    let cancelled = false;
    let timer: number | undefined;
    setView(INITIAL_PLAYER_OVERLAY);

    const load = async () => {
      try {
        const overlay = await playerOverlayApi.get({ params: { token } });
        if (!cancelled) setView({ status: "live", overlay });
      } catch (error) {
        if (cancelled) return;
        // A revoked link stays dead; anything else is a blip, so keep the last card on screen.
        if (isNotFoundError(error)) {
          setView({ status: "invalid", overlay: null });
          return;
        }
        setView((current) => ({ ...current, status: "offline" }));
      }
      if (!cancelled) timer = window.setTimeout(() => void load(), PLAYER_OVERLAY_POLL_MS);
    };

    void load();

    return () => {
      cancelled = true;
      window.clearTimeout(timer);
    };
  }, [token]);

  return view;
}