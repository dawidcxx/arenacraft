import type { ReactNode } from "react";
import { ClassIcon } from "@/components/ClassIcon";
import { Tag } from "@/components/Tag";
import { TwitchIcon } from "@/components/TwitchIcon";
import { HALL_OF_FAME, type Champion } from "@/lib/hallOfFame";
import { CLASS_COLORS } from "@/lib/ladder";

/** Tournament champions, newest first. */
export function HallOfFame() {
  return (
    <div className="grid md:grid-cols-2">
      {HALL_OF_FAME.map((champion) => (
        <ChampionCard key={champion.id} champion={champion} />
      ))}
    </div>
  );
}

function ChampionCard({ champion }: { champion: Champion }) {
  const color = CLASS_COLORS[champion.wowClass];

  return (
    <article className="grid content-start gap-4 border-b-2 border-divider px-4 py-6 md:border-r-2 md:even:border-r-0">
      <div className="flex flex-wrap items-start justify-between gap-2">
        <p className="text-xs tracking-[0.1em] text-label uppercase">Champion · {champion.date}</p>
        <Tag tone="outline">Winner</Tag>
      </div>

      <div className="flex items-center gap-3">
        <ClassIcon wowClass={champion.wowClass} className="size-14" />
        <div className="grid">
          <h3
            className="text-[44px] leading-none font-extrabold tracking-[-0.03em] md:text-[56px]"
            style={{ color }}
          >
            {champion.winner}
          </h3>
          <span className="text-base font-bold" style={{ color }}>
            {champion.spec === null ? champion.wowClass : `${champion.spec} ${champion.wowClass}`}
          </span>
        </div>
      </div>

      <dl className="border-t-2 border-divider">
        <Row term="Tournament">"{champion.tournament}"</Row>
        <Row term="Format">{champion.format}</Row>
        <Row term="Prize">{champion.prize}</Row>
      </dl>

      {champion.note !== undefined && (
        <p className="border-l-4 border-accent bg-surface-raised px-3 py-2.5 text-[15px] leading-[1.55]">
          <span className="mb-1 block text-xs font-extrabold tracking-[0.08em] text-label uppercase">
            Prize donated
          </span>
          {champion.note}
        </p>
      )}

      {champion.twitch !== undefined && (
        <a
          href={`https://www.twitch.tv/${champion.twitch}`}
          className="inline-flex w-fit items-center gap-2 bg-[#9146ff] px-3 py-1.5 text-sm font-extrabold text-white transition-[filter] hover:brightness-110"
        >
          <TwitchIcon className="size-4" />
          twitch.tv/{champion.twitch}
        </a>
      )}
    </article>
  );
}

function Row({ term, children }: { term: string; children: ReactNode }) {
  return (
    <div className="grid grid-cols-[110px_1fr] gap-2 border-b border-divider py-2.5 last:border-b-0">
      <dt className="pt-[3px] text-xs leading-[1.55] tracking-[0.08em] uppercase">{term}</dt>
      <dd className="text-[15px] leading-[1.55] font-bold">{children}</dd>
    </div>
  );
}
