import "@/global.css";
import type { LadderEntry } from "@arenacraft/common";
import { StrictMode, useEffect, useState } from "react";
import { createRoot } from "react-dom/client";
import { ladderApi } from "@/lib/apis";

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <App />
  </StrictMode>,
);

type LadderState =
  | { status: "loading" }
  | { status: "error"; message: string }
  | { status: "ready"; entries: LadderEntry[] };

function App() {
  const [state, setState] = useState<LadderState>({ status: "loading" });

  useEffect(() => {
    let cancelled = false;

    ladderApi
      .soloq()
      .then((entries) => {
        if (!cancelled) setState({ status: "ready", entries });
      })
      .catch((error: unknown) => {
        if (!cancelled) {
          setState({
            status: "error",
            message: error instanceof Error ? error.message : "Failed to load the ladder",
          });
        }
      });

    return () => {
      cancelled = true;
    };
  }, []);

  return (
    <main className="mx-auto min-h-screen w-full max-w-5xl p-6">
      <header className="mb-6">
        <h1 className="text-3xl font-bold">SoloQ Ladder</h1>
        <p className="text-sm text-gray-500">ArenaCraft 5v5 solo queue</p>
      </header>

      {state.status === "loading" && <p className="text-gray-500">Loading ladder…</p>}

      {state.status === "error" && (
        <p className="rounded bg-red-50 p-3 text-sm text-red-700">{state.message}</p>
      )}

      {state.status === "ready" && <LadderTable entries={state.entries} />}
    </main>
  );
}

function LadderTable({ entries }: { entries: LadderEntry[] }) {
  if (entries.length === 0) {
    return <p className="text-sm text-gray-500">No games played yet.</p>;
  }

  return (
    <div className="overflow-x-auto rounded border border-gray-200">
      <table className="w-full text-left text-sm">
        <thead className="bg-gray-50 text-xs uppercase text-gray-500">
          <tr>
            <th className="px-3 py-2">#</th>
            <th className="px-3 py-2">Name</th>
            <th className="px-3 py-2">Class</th>
            <th className="px-3 py-2">Role</th>
            <th className="px-3 py-2 text-right">Rating</th>
            <th className="px-3 py-2 text-right">W-L</th>
            <th className="px-3 py-2 text-right">Winrate</th>
          </tr>
        </thead>
        <tbody>
          {entries.map((entry, index) => {
            const games = entry.wins + entry.losses;
            const winrate = games === 0 ? "—" : `${Math.round((entry.wins / games) * 100)}%`;

            return (
              <tr key={`${entry.name}-${index}`} className="border-t border-gray-100">
                <td className="px-3 py-2 text-gray-500">{index + 1}</td>
                <td className="px-3 py-2 font-medium">{entry.name}</td>
                <td className="px-3 py-2">{entry.class}</td>
                <td className="px-3 py-2">{entry.role || "—"}</td>
                <td className="px-3 py-2 text-right tabular-nums">{entry.rating}</td>
                <td className="px-3 py-2 text-right tabular-nums">
                  {entry.wins}-{entry.losses}
                </td>
                <td className="px-3 py-2 text-right tabular-nums">{winrate}</td>
              </tr>
            );
          })}
        </tbody>
      </table>
    </div>
  );
}