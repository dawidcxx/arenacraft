import type { ReactNode } from "react";
import { Link } from "react-router-dom";
import { HallOfFame } from "@/components/HallOfFame";
import { PageHero } from "@/components/PageHero";
import { SectionIntro } from "@/components/SectionIntro";
import { TournamentCard } from "@/components/sections/TournamentCard";
import { cn } from "@/lib/cn";
import { TOURNAMENT_RULES, upcomingTournaments } from "@/lib/tournaments";

export function TournamentsPage() {
  const upcoming = upcomingTournaments();

  return (
    <>
      <PageHero eyebrow="Weekend SoloQ" title="Tournaments" />

      <SectionHeader title="Upcoming" />
      {upcoming.length === 0 ? (
        <p className="border-b-2 border-divider px-4 py-8 text-[15px] leading-[1.55] text-ink/70">
          No tournament is scheduled right now. Watch Discord for the next one.
        </p>
      ) : (
        <section className={cn("grid border-b-2 border-divider", upcoming.length > 1 && "md:grid-cols-2")}>
          {upcoming.map((tournament) => (
            <TournamentCard key={tournament.id} tournament={tournament} />
          ))}
        </section>
      )}

      <section className="grid border-b-2 border-divider md:grid-cols-2">
        <div className="border-b-2 border-divider px-4 py-6 md:border-r-2 md:border-b-0">
          <SectionIntro index="Rules" title="Notes" />
        </div>

        <ol>
          {TOURNAMENT_RULES.map((rule, index) => (
            <li
              key={rule}
              className="grid grid-cols-[36px_1fr] gap-2 border-b border-divider px-4 py-3 text-[15px] leading-[1.5] last:border-b-0"
            >
              <span className="font-extrabold text-accent">
                {String(index + 1).padStart(2, "0")}
              </span>
              <span>{rule}</span>
            </li>
          ))}
        </ol>
      </section>

      <SectionHeader
        id="hall-of-fame"
        title="Hall of fame"
        aside={
          <Link to="/hall-of-fame" className="text-[13px] text-label hover:underline">
            Open the hall of fame →
          </Link>
        }
      />
      <section className="border-b-2 border-divider">
        <HallOfFame />
      </section>
    </>
  );
}

function SectionHeader({
  id,
  title,
  aside,
}: {
  id?: string;
  title: string;
  aside?: ReactNode;
}) {
  return (
    <div
      id={id}
      className="flex scroll-mt-20 flex-wrap items-baseline gap-x-3 gap-y-1 border-b-2 border-divider px-4 py-3"
    >
      <h2 className="text-[28px] leading-[1.55] font-extrabold">{title}</h2>
      {aside !== undefined && <span className="ml-auto">{aside}</span>}
    </div>
  );
}
