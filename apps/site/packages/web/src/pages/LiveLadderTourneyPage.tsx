import { useEffect } from "react";
import { LiveLadderTable } from "@/components/LiveLadderTable";
import { useLiveLadder } from "@/hooks/useLiveLadder";
import { useTourneyClock } from "@/hooks/useTourneyClock";
import { cn } from "@/lib/cn";
import type { LiveLadderStatus } from "@/lib/liveLadder";
import { NEXT_TOURNAMENT } from "@/lib/site";
import { tourneyClockLabel } from "@/lib/tourneyClock";

const STATUS_STYLES: Record<LiveLadderStatus, { dot: string; label: string }> = {
  live: { dot: "bg-emerald-400", label: "Live" },
  connecting: { dot: "bg-amber-400", label: "Sync" },
  offline: { dot: "bg-red-500", label: "Offline" },
};

/**
 * Unlisted OBS browser source: the live top 5 plus a compact tourney reminder strip whose clock
 * follows `NEXT_TOURNAMENT`'s window. Same transparent chrome-less layout as `/live-ladder`.
 */
export function LiveLadderTourneyPage() {
  const { status, players } = useLiveLadder();
  const clock = useTourneyClock(NEXT_TOURNAMENT.startsAt, NEXT_TOURNAMENT.endsAt);
  const indicator = STATUS_STYLES[status];

  useEffect(() => {
    document.body.classList.add("live-ladder-obs");
    return () => document.body.classList.remove("live-ladder-obs");
  }, []);

  return (
    <main className="flex min-h-screen items-start justify-center p-2">
      {/* Opaque panel: OBS composites the transparent page over the stream, so a background with
          alpha (e.g. `bg-surface/95`) would make the whole table look see-through. */}
      <section className="w-full max-w-[380px] overflow-hidden border-2 border-divider bg-surface shadow-[0_10px_30px_rgba(0,0,0,0.55)]">
        <div className="flex items-center gap-2 border-b-2 border-divider px-3 py-2">
          <span className="size-2.5 shrink-0 bg-accent" aria-hidden="true" />
          <h1 className="truncate text-[11px] font-extrabold tracking-[0.16em] uppercase">
            Arenacraft top 5
          </h1>
          <span className="ml-auto flex shrink-0 items-center gap-1.5 text-[9px] tracking-[0.12em] text-ink/60 uppercase">
            <span className={cn("size-1.5 rounded-full", indicator.dot)} aria-hidden="true" />
            {indicator.label}
          </span>
        </div>

        <LiveLadderTable players={players} />

        <footer className="flex items-center gap-2 border-t-2 border-divider bg-accent px-3 py-1.5 text-white">
          <span className="size-2 shrink-0 bg-white" aria-hidden="true" />
          <span className="shrink-0 text-[10px] font-extrabold tracking-[0.14em] uppercase">
            {NEXT_TOURNAMENT.windowLabel}
          </span>
          <span
            className="ml-auto text-[11px] font-extrabold whitespace-nowrap tabular-nums"
            aria-label="Tourney clock"
          >
            {tourneyClockLabel(clock)}
          </span>
        </footer>
      </section>
    </main>
  );
}
