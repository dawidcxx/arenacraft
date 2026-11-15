import { Link } from "react-router-dom";
import { HallOfFame } from "@/components/HallOfFame";
import { PageHero } from "@/components/PageHero";

export function HallOfFamePage() {
  return (
    <>
      <PageHero eyebrow="Tournament champions" title="Hall of fame" />

      <section className="grid border-b-2 border-divider md:grid-cols-2">
        <p className="border-b-2 border-divider p-4 text-lg leading-[1.5] md:border-r-2 md:border-b-0">
          Every Arenacraft tournament has one winner: the highest rated player when the clock runs out.
          These are the champions so far.
        </p>
        <div className="flex items-center p-4">
          <Link
            to="/tournaments"
            className="inline-flex items-center bg-accent px-3.5 py-2 text-sm font-extrabold text-surface transition-[filter] hover:brightness-110"
          >
            Next tournaments →
          </Link>
        </div>
      </section>

      <section className="border-b-2 border-divider">
        <HallOfFame />
      </section>
    </>
  );
}
