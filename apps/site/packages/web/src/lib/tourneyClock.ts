export type TourneyPhase = "upcoming" | "live" | "finished";

export type TourneyClock = {
  phase: TourneyPhase;
  /** Seconds until the start (upcoming) or the end (live); zero once finished. */
  remainingSeconds: number;
};

/** Where the tourney clock stands for a given instant. */
export function tourneyClock(now: number, startsAt: Date, endsAt: Date): TourneyClock {
  if (now < startsAt.getTime()) {
    return {
      phase: "upcoming",
      remainingSeconds: Math.floor((startsAt.getTime() - now) / 1000),
    };
  }

  if (now < endsAt.getTime()) {
    return { phase: "live", remainingSeconds: Math.floor((endsAt.getTime() - now) / 1000) };
  }

  return { phase: "finished", remainingSeconds: 0 };
}

/** `3d 04h`, `3h 42m`, `42m 09s` — for a countdown that ticks every second. */
export function formatRemaining(totalSeconds: number): string {
  const seconds = Math.max(0, Math.floor(totalSeconds));
  const days = Math.floor(seconds / 86_400);
  const hours = Math.floor((seconds % 86_400) / 3_600);
  const minutes = Math.floor((seconds % 3_600) / 60);
  const rest = seconds % 60;

  if (days > 0) return `${days}d ${String(hours).padStart(2, "0")}h`;
  if (hours > 0) return `${hours}h ${String(minutes).padStart(2, "0")}m`;
  return `${minutes}m ${String(rest).padStart(2, "0")}s`;
}

/** Label for the tourney widget's clock slot. */
export function tourneyClockLabel({ phase, remainingSeconds }: TourneyClock): string {
  if (phase === "upcoming") return `Starts in ${formatRemaining(remainingSeconds)}`;
  if (phase === "live") return `Live · ${formatRemaining(remainingSeconds)} left`;
  return "Finished";
}
