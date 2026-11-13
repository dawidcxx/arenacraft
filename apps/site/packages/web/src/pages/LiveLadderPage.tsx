import { useEffect, useRef } from "react";
import { ClassIcon } from "@/components/ClassIcon";
import { useLiveLadder } from "@/hooks/useLiveLadder";
import { useRowMotion } from "@/hooks/useRowMotion";
import { cn } from "@/lib/cn";
import type { LiveLadderStatus } from "@/lib/liveLadder";

const STATUS_STYLES: Record<LiveLadderStatus, { dot: string; label: string }> = {
  live: { dot: "bg-emerald-400", label: "Live" },
  connecting: { dot: "bg-amber-400", label: "Sync" },
  offline: { dot: "bg-red-500", label: "Offline" },
};

// rank | class icon | player | rating | W-L
const ROW_GRID = "grid grid-cols-[1.75rem_1.75rem_minmax(0,1fr)_3.5rem_3.5rem] items-center";

/**
 * Unlinked OBS browser source. Transparent page background, compact enough for a narrow
 * capture region. Data arrives over `/api/live-ladder/ws`.
 */
export function LiveLadderPage() {
  const { status, players } = useLiveLadder();
  const listRef = useRef<HTMLDivElement>(null);
  const indicator = STATUS_STYLES[status];

  useRowMotion(listRef, players);

  useEffect(() => {
    document.body.classList.add("live-ladder-obs");
    return () => document.body.classList.remove("live-ladder-obs");
  }, []);

  return (
    <main className="flex min-h-screen items-start justify-center p-2">
      <section className="w-full max-w-[380px] overflow-hidden border-2 border-divider bg-surface/95 shadow-[0_10px_30px_rgba(0,0,0,0.55)]">
        <header className="flex items-center gap-2 border-b-2 border-divider px-3 py-2">
          <span className="size-2.5 shrink-0 bg-accent" aria-hidden="true" />
          <h1 className="truncate text-[11px] font-extrabold tracking-[0.16em] uppercase">
            Arenacraft top 5
          </h1>
          <span className="ml-auto flex shrink-0 items-center gap-1.5 text-[9px] tracking-[0.12em] text-ink/60 uppercase">
            <span className={cn("size-1.5 rounded-full", indicator.dot)} aria-hidden="true" />
            {indicator.label}
          </span>
        </header>

        <div role="table" aria-label="Arenacraft top 5" className="w-full">
          <div
            role="row"
            className={cn(ROW_GRID, "text-[9px] tracking-[0.1em] text-ink/45 uppercase")}
          >
            <div role="columnheader" className="px-2 py-1 font-bold">
              #
            </div>
            <div role="columnheader" aria-hidden="true" />
            <div role="columnheader" className="px-1 py-1 font-bold">
              Player
            </div>
            <div role="columnheader" className="px-2 py-1 text-right font-bold">
              Rating
            </div>
            <div role="columnheader" className="px-2 py-1 text-right font-bold">
              W-L
            </div>
          </div>

          <div ref={listRef} role="rowgroup">
            {players.length === 0 ? (
              <div role="row" className="border-t border-divider/60">
                <div role="cell" className="px-2 py-4 text-center text-[11px] text-ink/50">
                  Waiting for the ladder…
                </div>
              </div>
            ) : (
              players.map((player) => (
                <div
                  key={player.name}
                  data-row-key={player.name}
                  role="row"
                  className={cn(ROW_GRID, "border-t border-divider/60")}
                >
                  <div
                    role="cell"
                    className="px-2 py-1.5 text-[11px] font-extrabold text-accent tabular-nums"
                  >
                    {player.rank}
                  </div>
                  <div role="cell" className="py-1.5">
                    <ClassIcon wowClass={player.class} className="size-5" />
                  </div>
                  <div role="cell" className="min-w-0 truncate px-1 py-1.5 text-[13px] font-bold">
                    {player.name}
                  </div>
                  <div
                    role="cell"
                    className="px-2 py-1.5 text-right text-[13px] font-extrabold tabular-nums"
                  >
                    {player.rating}
                  </div>
                  <div
                    role="cell"
                    className="px-2 py-1.5 text-right text-[11px] whitespace-nowrap text-ink/65 tabular-nums"
                  >
                    {player.wins}-{player.losses}
                  </div>
                </div>
              ))
            )}
          </div>
        </div>
      </section>
    </main>
  );
}