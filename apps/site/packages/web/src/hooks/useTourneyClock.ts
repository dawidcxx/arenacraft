import { useEffect, useState } from "react";
import { tourneyClock, type TourneyClock } from "@/lib/tourneyClock";

/** Ticks every second so the tourney countdown stays in sync with the wall clock. */
export function useTourneyClock(startsAt: Date, endsAt: Date): TourneyClock {
  const [now, setNow] = useState(() => Date.now());

  useEffect(() => {
    const id = window.setInterval(() => setNow(Date.now()), 1_000);
    return () => window.clearInterval(id);
  }, []);

  return tourneyClock(now, startsAt, endsAt);
}
