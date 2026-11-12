import type { ReactNode } from "react";
import { Tag } from "@/components/Tag";
import { useCountdown } from "@/hooks/useCountdown";
import { cn } from "@/lib/cn";
import { formatCountdown } from "@/lib/countdown";
import type { Tournament } from "@/lib/tournaments";

export function TournamentCard({ tournament }: { tournament: Tournament }) {
  return (
    <article className="grid content-start gap-3 border-b-2 border-divider px-4 py-6 md:border-r-2 md:border-b-0 md:last:border-r-0">
      <div className="flex items-start justify-between gap-2">
        <p className="text-xs tracking-[0.1em] text-label uppercase">
          Tourney #{tournament.number}
        </p>
        <Tag tone={tournament.status === "finished" ? "neutral" : "outline"}>
          {tournament.status === "finished" ? "Finished" : "Upcoming"}
        </Tag>
      </div>

      <h2 className="text-[32px] leading-none font-extrabold tracking-[-0.03em] md:text-[48px]">
        "{tournament.title}"
      </h2>

      <p className="text-base leading-[1.55]">{tournament.description}</p>

      <dl className="border-t-2 border-divider">
        <DetailRow term="Date">{tournament.date}</DetailRow>
        <DetailRow term="Format">{tournament.format}</DetailRow>
        <DetailRow term="Starts in">
          <StartsIn startsAt={tournament.startsAt} />
        </DetailRow>
      </dl>

      <div className="grid border-2 border-divider md:grid-cols-2">
        {tournament.prizes.map((prize) => (
          <div
            key={prize.amount}
            className={cn(
              "p-3",
              prize.highlight &&
                "border-b-2 border-white bg-accent text-white md:border-r-2 md:border-b-0",
            )}
          >
            <div className="text-[40px] leading-none font-extrabold tracking-[-0.03em]">
              {prize.amount}
            </div>
            <div className="text-sm leading-[1.4]">{prize.description}</div>
          </div>
        ))}
      </div>
    </article>
  );
}

function DetailRow({ term, children }: { term: string; children: ReactNode }) {
  return (
    <div className="grid grid-cols-[110px_1fr] gap-2 border-b border-divider py-3 last:border-b-0">
      <dt className="pt-[3px] text-xs leading-[1.55] tracking-[0.08em] uppercase">{term}</dt>
      <dd className="text-[15px] leading-[1.55] font-bold">{children}</dd>
    </div>
  );
}

function StartsIn({ startsAt }: { startsAt: Date | null }) {
  if (startsAt === null) return <>Finished</>;
  return <StartsInCountdown startsAt={startsAt} />;
}

function StartsInCountdown({ startsAt }: { startsAt: Date }) {
  const countdown = useCountdown(startsAt);
  return <>{formatCountdown(countdown)}</>;
}