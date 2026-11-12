import heroImage from "@/assets/hero.webp";

export function PageHero({ eyebrow, title }: { eyebrow: string; title: string }) {
  return (
    <section className="relative flex min-h-[260px] items-end border-b-2 border-divider bg-surface">
      <img src={heroImage} alt="" className="absolute inset-0 h-full w-full object-cover" />
      <div className="absolute inset-0 bg-[linear-gradient(to_top,#0c0b0b_5%,rgba(12,11,11,0.55)_60%,rgba(12,11,11,0.25))]" />

      <div className="relative z-10 grid w-full gap-2 px-4 py-4">
        <p className="text-xs tracking-[0.1em] text-label uppercase">{eyebrow}</p>
        <h1 className="text-[56px] leading-[0.9] font-extrabold tracking-[-0.04em] sm:text-[72px] md:text-[96px]">
          {title}
        </h1>
      </div>
    </section>
  );
}