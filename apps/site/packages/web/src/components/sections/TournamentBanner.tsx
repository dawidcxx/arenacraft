import { Link } from "react-router-dom";
import { useCountdown } from "@/hooks/useCountdown";
import { formatCountdown } from "@/lib/countdown";
import { nextTournament, type Tournament } from "@/lib/tournaments";

export function TournamentBanner() {
  const tournament = nextTournament();

  // Expired tourneys disappear with the upcoming list; no banner until the next one is dated.
  if (tournament === undefined) return null;

  return <NextTournamentBanner tournament={tournament} />;
}

function NextTournamentBanner({ tournament }: { tournament: Tournament }) {
  const countdown = useCountdown(tournament.startsAt);

  return (
    <Link
      to="/tournaments"
      className="grid items-center gap-x-3 gap-y-1 border-b-2 border-divider bg-accent px-4 py-3 text-white transition-[filter] hover:brightness-105 md:grid-cols-[auto_1fr_auto]"
    >
      <span className="text-xs font-bold tracking-[0.1em] uppercase">Next tourney</span>
      <span className="text-lg leading-[1.55] font-extrabold tracking-[-0.01em] md:text-2xl">
        {tournament.title} · {tournament.shortDateLabel}
      </span>
      <span className="text-sm font-extrabold md:text-lg">{formatCountdown(countdown)} →</span>
    </Link>
  );
}
