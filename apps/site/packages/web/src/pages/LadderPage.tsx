import { useState } from "react";
import { PageHero } from "@/components/PageHero";
import { useLadder } from "@/hooks/useLadder";
import { cn } from "@/lib/cn";
import { CLASS_COLORS, LADDER_FILTERS, winRate, type LadderFilter } from "@/lib/ladder";

export function LadderPage() {
  const state = useLadder();
  const [filter, setFilter] = useState<LadderFilter>("All");

  const entries = state.status === "ready" ? state.entries : [];
  const visibleEntries = filter === "All" ? entries : entries.filter((e) => e.role === filter);
  const notice =
    state.status === "loading"
      ? "Loading the live ladder…"
      : state.status === "error"
        ? `Live ladder unavailable: ${state.message}`
        : "Live ladder from the server database. Refreshes every 60 seconds.";

  return (
    <>
      <PageHero eyebrow="3v3 SoloQ" title="Ladder" />

      <section className="border-b-2 border-divider">
        <div className="flex flex-wrap items-center gap-2 border-b-2 border-divider px-4 py-3">
          {LADDER_FILTERS.map((option) => (
            <button
              key={option}
              type="button"
              onClick={() => setFilter(option)}
              aria-pressed={filter === option}
              className={cn(
                "px-3.5 py-2 text-sm font-extrabold transition-colors",
                filter === option
                  ? "bg-accent text-surface"
                  : "border border-divider text-ink hover:bg-surface-raised",
              )}
            >
              {option}
            </button>
          ))}
          <div className="ml-auto text-[13px]">Showing top players by rating</div>
        </div>

        <div className="border-b-2 border-divider bg-[#2d2b2b] px-4 py-3 text-sm leading-[1.55]">
          {notice}
        </div>

        <div className="overflow-x-auto">
          <table className="w-full min-w-[720px] border-collapse text-left text-sm leading-[1.55]">
            <thead>
              <tr>
                {["#", "Player", "Class", "Role", "Rating", "W", "L", "Win %"].map((header) => (
                  <th
                    key={header}
                    scope="col"
                    className="border-b-2 border-divider p-2 text-[11px] font-bold tracking-[0.08em] text-ink/60 uppercase"
                  >
                    {header}
                  </th>
                ))}
              </tr>
            </thead>
            <tbody>
              {visibleEntries.length === 0 ? (
                <tr>
                  <td colSpan={8} className="p-6 text-center text-ink/60">
                    {state.status === "loading"
                      ? "Loading ladder…"
                      : state.status === "error"
                        ? "The ladder is not available right now."
                        : "No players match this role yet."}
                  </td>
                </tr>
              ) : (
                visibleEntries.map((entry, index) => (
                  <tr key={`${entry.name}-${index}`}>
                    <td className="border-b border-divider p-2 font-extrabold text-accent">
                      {index + 1}
                    </td>
                    <td className="border-b border-divider p-2 font-bold">{entry.name}</td>
                    <td
                      className="border-b border-divider p-2 font-bold"
                      style={{ color: CLASS_COLORS[entry.class] }}
                    >
                      {entry.class}
                    </td>
                    <td className="border-b border-divider p-2">{entry.role || "—"}</td>
                    <td className="border-b border-divider p-2 font-extrabold">{entry.rating}</td>
                    <td className="border-b border-divider p-2">{entry.wins}</td>
                    <td className="border-b border-divider p-2">{entry.losses}</td>
                    <td className="border-b border-divider p-2">{winRate(entry)}</td>
                  </tr>
                ))
              )}
            </tbody>
          </table>
        </div>
      </section>
    </>
  );
}