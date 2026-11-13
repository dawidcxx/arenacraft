import type { OverlayRecord } from "@arenacraft/common";
import { useEffect } from "react";
import { useParams } from "react-router-dom";
import { ClassIcon } from "@/components/ClassIcon";
import { usePlayerOverlay } from "@/hooks/usePlayerOverlay";
import { cn } from "@/lib/cn";
import { matchSlots, type OverlayMatchSlot, type PlayerOverlayStatus } from "@/lib/playerOverlay";

const STATUS_STYLES: Record<PlayerOverlayStatus, { dot: string; label: string }> = {
  live: { dot: "bg-emerald-400", label: "Live" },
  connecting: { dot: "bg-amber-400", label: "Sync" },
  offline: { dot: "bg-red-500", label: "Offline" },
  invalid: { dot: "bg-red-500", label: "Offline" },
};

const SLOT_STYLES: Record<"W" | "L" | "empty", string> = {
  W: "border-emerald-400/55 bg-emerald-400/12 text-emerald-400",
  L: "border-red-500/55 bg-red-500/12 text-red-500",
  empty: "border-divider/50 text-ink/25",
};

/**
 * Personal OBS browser source, one per streamer: `/overlay/<token>`, where the token comes from the
 * Discord bot's `/overlay` command. Transparent page background like the live ladder widget. Data
 * is polled from `/api/overlay/:token`.
 */
export function PlayerOverlayPage() {
  const { token = "" } = useParams();
  const { status, overlay } = usePlayerOverlay(token);
  const indicator = STATUS_STYLES[status];

  useEffect(() => {
    document.body.classList.add("player-overlay-obs");
    return () => document.body.classList.remove("player-overlay-obs");
  }, []);

  return (
    <main className="flex min-h-screen items-start justify-center p-2">
      <section className="w-full max-w-[380px] overflow-hidden border-2 border-divider bg-surface/95 shadow-[0_10px_30px_rgba(0,0,0,0.55)]">
        <header className="flex items-center gap-2 border-b-2 border-divider px-3 py-1.5">
          <span className="size-2.5 shrink-0 bg-accent" aria-hidden="true" />
          <h1 className="truncate text-[11px] font-extrabold tracking-[0.16em] uppercase">
            Arenacraft · Solo Q
          </h1>
          <span className="ml-auto flex shrink-0 items-center gap-1.5 text-[9px] tracking-[0.12em] text-ink/60 uppercase">
            <span className={cn("size-1.5 rounded-full", indicator.dot)} aria-hidden="true" />
            {indicator.label}
          </span>
        </header>

        {status === "invalid" ? (
          <p className="px-3 py-3 text-center text-[11px] text-ink/60">
            Overlay link not found. Run <span className="font-bold text-ink">/overlay</span> in
            Discord to get yours.
          </p>
        ) : overlay === null ? (
          <p className="px-3 py-3 text-center text-[11px] text-ink/50">Loading your stats…</p>
        ) : (
          <>
            <div className="flex items-center gap-2.5 px-3 py-2">
              <ClassIcon wowClass={overlay.class} className="size-8 shrink-0" />
              <div className="min-w-0">
                <div className="truncate text-[15px] leading-tight font-extrabold">
                  {overlay.name}
                </div>
                <div className="text-[9px] tracking-[0.12em] text-ink/45 uppercase">
                  {overlay.class}
                </div>
              </div>
              <div className="ml-auto text-right">
                <div className="text-[28px] leading-none font-extrabold tabular-nums">
                  {overlay.rating ?? "—"}
                </div>
                <div className="mt-0.5 text-[9px] tracking-[0.12em] text-ink/45 uppercase">
                  Rating
                </div>
              </div>
            </div>

            <div className="flex items-stretch border-t border-divider/60">
              <RecordCell label="All" record={overlay.all} />
              <RecordCell label="Today" record={overlay.today} />
              <div
                className="ml-auto flex items-center gap-1 px-3 py-1.5"
                aria-label="Last 5 SoloQ matches, newest last"
              >
                {matchSlots(overlay.lastMatches).map((slot, index, slots) => (
                  <MatchSlot key={index} slot={slot} newest={index === slots.length - 1} />
                ))}
              </div>
            </div>
          </>
        )}
      </section>
    </main>
  );
}

function RecordCell({ label, record }: { label: string; record: OverlayRecord }) {
  return (
    <div className="flex items-baseline gap-1.5 border-r border-divider/60 px-3 py-1.5">
      <span className="text-[9px] font-bold tracking-[0.1em] text-ink/45 uppercase">{label}</span>
      <span className="text-[14px] font-extrabold whitespace-nowrap tabular-nums">
        <span className="text-emerald-400">{record.wins}</span>
        <span className="mx-px text-ink/35">:</span>
        <span className="text-red-500">{record.losses}</span>
      </span>
    </div>
  );
}

function MatchSlot({ slot, newest }: { slot: OverlayMatchSlot; newest: boolean }) {
  return (
    <span
      className={cn(
        "grid h-5 w-[22px] place-items-center border-2 text-[11px] font-extrabold",
        SLOT_STYLES[slot ?? "empty"],
        newest && slot !== null && "outline-2 outline-offset-1 outline-accent",
      )}
    >
      {slot ?? "–"}
    </span>
  );
}