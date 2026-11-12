import { PageHero } from "@/components/PageHero";
import { SectionIntro } from "@/components/SectionIntro";
import { TournamentCard } from "@/components/sections/TournamentCard";
import { TOURNAMENT_NOTE, TOURNAMENT_RULES, TOURNAMENTS } from "@/lib/tournaments";

export function TournamentsPage() {
  return (
    <>
      <PageHero eyebrow="Weekend SoloQ" title="Tournaments" />

      <section className="grid border-b-2 border-divider md:grid-cols-2">
        {TOURNAMENTS.map((tournament) => (
          <TournamentCard key={tournament.id} tournament={tournament} />
        ))}
      </section>

      <div className="border-b-2 border-divider px-4 py-3 text-base leading-[1.55]">
        {TOURNAMENT_NOTE}
      </div>

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
    </>
  );
}