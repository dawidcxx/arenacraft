import { useEffect, useState } from "react";
import { clockFromSecondsOfDay, eventClock, type EventClock } from "@/lib/tourneyAgenda";

// `?at=19:30` freezes the banner at that tourney-day time (still ticking) so a phase can be
// previewed before it happens. Handy when checking the cover screen before going live.
const previewStart = previewOffsetSeconds();
const previewEpoch = performance.now();

/** Event-timezone clock that ticks every second. */
export function useEventClock(): EventClock {
  const [clock, setClock] = useState(currentClock);

  useEffect(() => {
    const id = window.setInterval(() => setClock(currentClock()), 1_000);
    return () => window.clearInterval(id);
  }, []);

  return clock;
}

function currentClock(): EventClock {
  if (previewStart === null) return eventClock(new Date());
  return clockFromSecondsOfDay(previewStart + (performance.now() - previewEpoch) / 1000);
}

function previewOffsetSeconds(): number | null {
  const at = new URLSearchParams(window.location.search).get("at");
  if (at === null) return null;

  const match = /^(\d{1,2}):(\d{2})(?::(\d{2}))?$/.exec(at);
  if (match === null) return null;

  const hours = Number(match[1]);
  const minutes = Number(match[2]);
  const seconds = Number(match[3] ?? 0);
  if (hours > 23 || minutes > 59 || seconds > 59) return null;

  return hours * 3600 + minutes * 60 + seconds;
}
