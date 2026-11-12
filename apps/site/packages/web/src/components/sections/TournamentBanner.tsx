import { Link } from "react-router-dom";
import { useCountdown } from "@/hooks/useCountdown";
import { formatCountdown } from "@/lib/countdown";
import { NEXT_TOURNAMENT } from "@/lib/site";

export function TournamentBanner() {
  const countdown = useCountdown(NEXT_TOURNAMENT.startsAt);

  return (
    <Link
      to="/tournaments"
      className="grid items-center gap-x-3 gap-y-1 border-b-2 border-divider bg-accent px-4 py-3 text-white transition-[filter] hover:brightness-105 md:grid-cols-[auto_1fr_auto]"
    >
      <span className="text-xs font-bold tracking-[0.1em] uppercase">Next tourney</span>
      <span className="text-lg leading-[1.55] font-extrabold tracking-[-0.01em] md:text-2xl">
        {NEXT_TOURNAMENT.name} · Sun 15 Nov, 18:00 CET
      </span>
      <span className="text-sm font-extrabold md:text-lg">{formatCountdown(countdown)} →</span>
    </Link>
  );
}