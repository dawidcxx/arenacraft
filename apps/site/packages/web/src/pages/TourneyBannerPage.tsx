import { useEffect, useState } from "react";
import { ClassIcon } from "@/components/ClassIcon";
import { useEventClock } from "@/hooks/useEventClock";
import { cn } from "@/lib/cn";
import { HALL_OF_FAME } from "@/lib/hallOfFame";
import { CLASS_COLORS } from "@/lib/ladder";
import { DISCORD_LABEL, NEXT_TOURNAMENT } from "@/lib/site";
import { agendaStatus, eventDayLabel, TOURNEY_AGENDA, tourneyPhase } from "@/lib/tourneyAgenda";
import { TOURNAMENTS } from "@/lib/tournaments";

// 16:9 cover, designed at 1080p and scaled to whatever the OBS browser source is.
const STAGE_WIDTH = 1920;
const STAGE_HEIGHT = 1080;

/**
 * Unlisted full-frame OBS cover for stream start / breaks / after-show: event clock, the tourney
 * agenda with the current segment highlighted, and previous champions. Everything is static data
 * from `lib/` (no API calls).
 */
export function TourneyBannerPage() {
  const clock = useEventClock();
  const phase = tourneyPhase(clock.secondsOfDay);
  const scale = useStageScale();
  const current = TOURNAMENTS.find((tournament) => tournament.status === "upcoming");

  const statuses = TOURNEY_AGENDA.map((row) => agendaStatus(row, clock.secondsOfDay));
  const nextIndex = statuses.indexOf("upcoming");
  const agenda = TOURNEY_AGENDA.map((row, index) => {
    const status = statuses[index];
    const label =
      status === "live" ? "Now" : status === "done" ? "Done" : index === nextIndex ? "Next" : "";
    return { row, status, label };
  });

  useEffect(() => {
    document.body.classList.add("tourney-banner-obs");
    return () => document.body.classList.remove("tourney-banner-obs");
  }, []);

  return (
    <main className="relative h-screen w-screen overflow-hidden bg-surface">
      <div
        className="absolute top-1/2 left-1/2 flex h-[1080px] w-[1920px] flex-col bg-surface px-[72px] pt-[68px] pb-[44px] text-ink"
        style={{ transformOrigin: "0 0", transform: `scale(${scale}) translate(-50%, -50%)` }}
      >
        <span className="absolute inset-x-0 top-0 h-[8px] bg-accent" aria-hidden="true" />

        <header className="flex items-center justify-between">
          <div className="flex items-center gap-[18px]">
            <span className="size-[22px] bg-accent" aria-hidden="true" />
            <span className="text-[26px] font-extrabold tracking-[0.34em]">ARENACRAFT</span>
            <span className="h-[22px] w-[2px] bg-divider" aria-hidden="true" />
            <span className="text-[19px] font-bold tracking-[0.3em] text-label uppercase">
              {current === undefined ? "Tourney" : `Tourney #${current.number}`}
            </span>
          </div>
          <p className="text-[17px] font-bold tracking-[0.24em] text-ink/50 uppercase">
            {eventDayLabel(NEXT_TOURNAMENT.startsAt)} · {NEXT_TOURNAMENT.windowLabel}
          </p>
        </header>

        <div className="grid flex-1 grid-cols-[1.15fr_1fr] items-center gap-[72px]">
          <div className="flex flex-col items-start">
            <span
              className={cn(
                "inline-flex items-center gap-[12px] border-2 px-[18px] py-[8px] text-[16px] font-extrabold tracking-[0.26em] uppercase",
                phase.live ? "border-accent bg-accent text-white" : "border-divider text-label",
              )}
            >
              <span
                className={cn("size-[10px] rounded-full", phase.live ? "bg-white" : "bg-accent")}
                aria-hidden="true"
              />
              {phase.kicker}
            </span>

            <div className="mt-[26px] flex items-baseline gap-[16px]">
              <span className="text-[212px] leading-[0.85] font-extrabold tracking-[-0.045em] tabular-nums">
                {clock.label}
              </span>
              <span className="text-[26px] font-bold tracking-[0.24em] text-ink/40">CET</span>
            </div>

            <span className="mt-[30px] h-[3px] w-[150px] bg-accent" aria-hidden="true" />

            <h1 className="mt-[26px] text-[62px] leading-[1.05] font-extrabold tracking-[-0.02em]">
              {phase.headline}
            </h1>
            <p className="mt-[10px] text-[22px] leading-snug text-ink/55">{phase.caption}</p>

            {phase.progress !== null && (
              <div className="mt-[26px] h-[8px] w-[520px] bg-surface-raised" aria-hidden="true">
                <div
                  className="h-full bg-accent"
                  style={{ width: `${Math.round(Math.min(1, Math.max(0, phase.progress)) * 100)}%` }}
                />
              </div>
            )}
          </div>

          <div className="flex flex-col border-l-2 border-divider pl-[56px]">
            <div className="flex items-baseline justify-between pb-[16px]">
              <p className="text-[15px] font-extrabold tracking-[0.28em] text-label uppercase">
                Tonight
              </p>
            </div>
            <div className="border-t-2 border-divider">
              {agenda.map(({ row, status, label }) => (
                <div
                  key={row.id}
                  className={cn(
                    "grid grid-cols-[238px_1fr_64px] items-center gap-[16px] border-b border-divider/60 px-[18px] py-[26px]",
                    status === "live" && "bg-accent text-white",
                  )}
                >
                  <span
                    className={cn(
                      "text-[23px] font-extrabold tabular-nums",
                      status === "done" && "text-ink/30",
                      status === "upcoming" && "text-ink/75",
                    )}
                  >
                    {row.range}
                  </span>
                  <span
                    className={cn(
                      "text-[23px] font-bold",
                      status === "done" && "text-ink/30",
                      status === "upcoming" && "text-ink/85",
                    )}
                  >
                    {row.title}
                  </span>
                  <span
                    className={cn(
                      "text-right text-[13px] font-extrabold tracking-[0.14em] uppercase",
                      status === "live" ? "text-white" : status === "upcoming" ? "text-accent" : "text-ink/30",
                    )}
                  >
                    {label}
                  </span>
                </div>
              ))}
            </div>
          </div>
        </div>

        <section className="border-t-2 border-divider pt-[30px]">
          <p className="text-[15px] font-extrabold tracking-[0.28em] text-label uppercase">
            Previous winners
          </p>
          <div className="mt-[18px] grid grid-cols-2 gap-[64px]">
            {HALL_OF_FAME.map((champion) => (
              <article
                key={champion.id}
                className="flex items-center gap-[20px] border-l-4 border-accent pl-[20px]"
              >
                <ClassIcon wowClass={champion.wowClass} className="size-[48px]" />
                <div className="min-w-0">
                  <p
                    className="text-[38px] leading-none font-extrabold tracking-[-0.02em]"
                    style={{ color: CLASS_COLORS[champion.wowClass] }}
                  >
                    {champion.winner}
                  </p>
                  <p className="mt-[7px] truncate text-[15px] font-bold text-ink/50">
                    {champion.spec === null
                      ? champion.wowClass
                      : `${champion.spec} ${champion.wowClass}`}{" "}
                    · {champion.tournament} · {champion.prize}
                  </p>
                </div>
              </article>
            ))}
          </div>
        </section>

        <footer className="flex items-center justify-between pt-[30px] text-[13px] font-bold tracking-[0.22em] text-ink/35 uppercase">
          <p>arenacraft.fun · {DISCORD_LABEL}</p>
          <p>{current?.format ?? ""}</p>
        </footer>
      </div>
    </main>
  );
}

/** Fits the fixed 1920x1080 stage into the viewport, letterboxed by the page background. */
function useStageScale(): number {
  const [scale, setScale] = useState(() => stageScale());

  useEffect(() => {
    const update = () => setScale(stageScale());

    update();
    window.addEventListener("resize", update);
    return () => window.removeEventListener("resize", update);
  }, []);

  return scale;
}

function stageScale(): number {
  return Math.min(window.innerWidth / STAGE_WIDTH, window.innerHeight / STAGE_HEIGHT);
}
