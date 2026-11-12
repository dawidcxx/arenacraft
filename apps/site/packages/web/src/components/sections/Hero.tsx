import heroImage from "@/assets/hero.webp";
import { DiscordIcon } from "@/components/DiscordIcon";
import { DISCORD_URL } from "@/lib/site";

export function Hero() {
  return (
    <section
      id="home"
      className="relative flex min-h-[560px] items-end border-b-2 border-divider bg-surface md:min-h-[720px]"
    >
      <img
        src={heroImage}
        alt=""
        width={1672}
        height={941}
        className="absolute inset-0 h-full w-full object-cover"
      />
      <div className="absolute inset-0 bg-[linear-gradient(to_top,#0c0b0b_0%,rgba(12,11,11,0.7)_30%,rgba(12,11,11,0.1)_70%)]" />

      <div className="relative z-10 grid w-full gap-4 px-4 pt-8 pb-6">
        <div className="flex flex-wrap items-center gap-2 text-xs tracking-[0.1em] uppercase">
          <span>Wrath of the Lich King</span>
          <span aria-hidden="true">/</span>
          <span>Private arena server</span>
        </div>

        <h1 className="text-[clamp(56px,11vw,148px)] leading-[0.88] font-extrabold tracking-[-0.04em]">
          <span className="block">Arenacraft.</span>
          <span className="block">3v3 Solo Queue.</span>
        </h1>

        <p className="max-w-[560px] text-[19px] leading-[1.45]">
          264 ilvl meta with 277 ilvl weapons. No professions. Queue alone as melee, caster or
          healer and get matched into a 3v3.
        </p>

        <div className="flex justify-center pt-3">
          <a
            href={DISCORD_URL}
            className="flex w-full max-w-[640px] items-center justify-center gap-4 bg-discord px-6 py-5 text-[clamp(1.5rem,3vw,2.25rem)] font-extrabold tracking-[-0.02em] text-white shadow-[0_12px_40px_rgba(88,101,242,0.45)] transition-[filter] hover:brightness-110 sm:gap-5 sm:px-9 sm:py-7"
          >
            <DiscordIcon className="size-8 shrink-0 sm:size-12" />
            Join the Discord
          </a>
        </div>
      </div>
    </section>
  );
}