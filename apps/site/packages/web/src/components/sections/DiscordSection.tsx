import { SectionIntro } from "@/components/SectionIntro";
import { cn } from "@/lib/cn";

const STEPS = [
  {
    title: "Join the Waiting Room",
    body: "Hop into the Waiting Room voice channel and queue in game.",
  },
  {
    title: "Get matched",
    body: "When the game pops, the bot moves you and your team into your own room automatically.",
  },
  {
    title: "Play together",
    body: "Talk to your teammates through the whole match, then head back to the Waiting Room for the next one.",
  },
] as const;

export function DiscordSection() {
  return (
    <section className="grid border-b-2 border-divider md:grid-cols-2">
      <div className="border-b-2 border-divider px-4 py-6 md:border-r-2 md:border-b-0">
        <SectionIntro index="02 · Discord" title="Built around Discord" />
        <p className="mt-3 max-w-[460px] text-[17px] leading-[1.5]">
          Arenacraft runs on our Discord server. When a SoloQ game starts, you are matched into the
          arena and into a voice channel with your two teammates, so you can call swaps, coordinate
          CC and have more fun playing together.
        </p>
      </div>

      <div>
        {STEPS.map((step, index) => (
          <div
            key={step.title}
            className={cn(
              "grid grid-cols-[48px_1fr] gap-2 p-4",
              index < STEPS.length - 1 && "border-b-2 border-divider",
            )}
          >
            <div className="text-[28px] leading-[1.55] font-extrabold text-accent">{index + 1}</div>
            <div>
              <h3 className="text-lg leading-[1.55] font-bold">{step.title}</h3>
              <p className="mt-1 text-[15px] leading-[1.55]">{step.body}</p>
            </div>
          </div>
        ))}
      </div>
    </section>
  );
}