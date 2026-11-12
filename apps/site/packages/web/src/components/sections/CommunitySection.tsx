import { SectionIntro } from "@/components/SectionIntro";

export function CommunitySection() {
  return (
    <section className="grid border-b-2 border-divider md:grid-cols-2">
      <div className="border-b-2 border-divider px-4 py-6 md:border-r-2 md:border-b-0">
        <SectionIntro index="01 · Community" title="A friendly community. Everyone is welcome." />
      </div>

      <div className="grid content-start gap-3 px-4 py-6">
        <p className="text-lg leading-[1.55]">
          Arenacraft is a community project built by players who love WotLK arena. Whether you were
          Gladiator in 2009 or you are queueing your first 3v3 today, there is a place for you here.
        </p>
        <p className="text-base leading-[1.55]">
          New players get help, not flame. Ask questions in Discord, jump into voice with your team
          and learn by playing. We keep it fun for every skill level.
        </p>
      </div>
    </section>
  );
}