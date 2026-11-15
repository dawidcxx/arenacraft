import type { LiveLadderPlayer } from "@arenacraft/common";
import { useRef } from "react";
import { ClassIcon } from "@/components/ClassIcon";
import { useRowMotion } from "@/hooks/useRowMotion";
import { cn } from "@/lib/cn";

// rank | class icon | player | rating | W-L
const ROW_GRID = "grid grid-cols-[1.75rem_1.75rem_minmax(0,1fr)_3.5rem_3.5rem] items-center";

/**
 * The top-5 table shared by the `/live-ladder` widgets. Rows animate on rank changes.
 */
export function LiveLadderTable({ players }: { players: LiveLadderPlayer[] }) {
  const listRef = useRef<HTMLDivElement>(null);

  useRowMotion(listRef, players);

  return (
    <div role="table" aria-label="Arenacraft top 5" className="w-full">
      <div role="row" className={cn(ROW_GRID, "text-[9px] tracking-[0.1em] text-ink/45 uppercase")}>
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
  );
}
