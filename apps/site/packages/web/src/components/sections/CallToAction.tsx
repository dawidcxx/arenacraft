import { DISCORD_LABEL, DISCORD_URL } from "@/lib/site";

export function CallToAction() {
  return (
    <section className="grid gap-4 bg-accent px-4 py-8 text-white">
      <h2 className="text-[clamp(3rem,8vw,6.5rem)] leading-[0.9] font-extrabold tracking-[-0.04em]">
        Queue up.
        <br />
        The Waiting Room is open.
      </h2>
      <a
        href={DISCORD_URL}
        className="inline-flex w-fit items-center gap-1.5 bg-surface px-3.5 py-2 text-sm font-extrabold text-ink transition-colors hover:bg-surface-raised"
      >
        {DISCORD_LABEL} <span aria-hidden="true">→</span>
      </a>
    </section>
  );
}