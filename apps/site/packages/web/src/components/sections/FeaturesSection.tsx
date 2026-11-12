import { SectionIntro } from "@/components/SectionIntro";
import { Tag } from "@/components/Tag";
import { cn } from "@/lib/cn";

const FEATURES = [
  {
    title: "Gear",
    description: "264 ilvl meta with 277 ilvl weapons. No profession, old AT like setup",
  },
  {
    title: "SoloQ",
    description: "Melee / Caster / Healer SoloQ",
  },
  {
    title: "Discord voice",
    description:
      "SoloQ is integrated with discord voice chat. Players inside the Waiting Room voice channel get auto assigned a room when a game starts",
  },
] as const;

export function FeaturesSection() {
  return (
    <section className="border-b-2 border-divider">
      <div className="border-b-2 border-divider px-4 pt-6 pb-4">
        <SectionIntro index="04 · Server" title="Features" className="gap-2" />
      </div>

      <div className="grid md:grid-cols-3">
        {FEATURES.map((feature, index) => (
          <article
            key={feature.title}
            className={cn(
              "grid content-start gap-2 p-4",
              index < FEATURES.length - 1 &&
                "border-b-2 border-divider md:border-r-2 md:border-b-0",
            )}
          >
            <Tag>Live</Tag>
            <h3 className="text-[22px] leading-[1.15] font-extrabold tracking-[-0.015em]">
              {feature.title}
            </h3>
            <p className="text-[15px] leading-[1.5]">{feature.description}</p>
          </article>
        ))}
      </div>

      <div className="grid gap-2 border-t-2 border-divider p-4">
        <Tag tone="outline">Planned</Tag>
        <h3 className="text-[22px] leading-[1.15] font-extrabold tracking-[-0.015em]">
          Weekend SoloQ tournaments
        </h3>
        <p className="text-[15px] leading-[1.5]">
          Format: Bracket reset -&gt; Ladder climb race. Per role (MCH) and lottery rewards to
          incentivize continued play during tourneys
        </p>
      </div>
    </section>
  );
}