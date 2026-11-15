import { PageHero } from "@/components/PageHero";
import { SectionIntro } from "@/components/SectionIntro";
import {
  ADDON_CATEGORIES,
  ADDON_INSTALL_STEPS,
  FEATURED_ADDON,
  type Addon,
  type AddonCommand,
} from "@/lib/addons";

export function AddonsPage() {
  const featured = FEATURED_ADDON;

  return (
    <>
      <PageHero eyebrow="Interface" title="Addons" />

      <section className="border-b-2 border-divider p-4">
        <p className="max-w-3xl text-lg leading-[1.5]">
          Our own spectator addon plus the arena addons the community plays with, packed and checked
          for the 3.3.5a client. Download, unzip into <code className="font-mono text-label">Interface/AddOns</code>,
          done.
        </p>
      </section>

      <section className="grid border-b-2 border-divider md:grid-cols-2">
        <div className="grid content-start gap-4 border-b-2 border-divider px-4 py-6 md:border-r-2 md:border-b-0">
          <p className="text-xs tracking-[0.1em] text-label uppercase">
            Made for Arenacraft · by {featured.author} · version {featured.version}
          </p>
          <h2 className="text-[36px] leading-none font-extrabold tracking-[-0.03em] md:text-[48px]">
            {featured.name}
          </h2>
          <p className="text-base leading-[1.55]">{featured.summary}</p>
          <DownloadButton addon={featured} large />
          {featured.features !== undefined && (
            <Features features={featured.features} className="border-t-2 border-divider pt-4" />
          )}
          {featured.credits !== undefined && <p className="text-xs text-ink/60">{featured.credits}</p>}
        </div>

        <div>
          <h3 className="border-b-2 border-divider px-4 py-3 text-[11px] font-bold tracking-[0.08em] text-ink/60 uppercase">
            Commands
          </h3>
          <Commands commands={featured.commands} />
        </div>
      </section>

      {ADDON_CATEGORIES.map((category) => (
        <section key={category.title} className="border-b-2 border-divider">
          <h2 className="border-b-2 border-divider px-4 py-3 text-[28px] leading-[1.55] font-extrabold">
            {category.title}
          </h2>
          {/* Per-card right/bottom borders; the outer edges are clipped so a short last row leaves no filler. */}
          <div className="overflow-hidden">
            <div className="-mr-px -mb-px grid md:grid-cols-2 xl:grid-cols-3">
              {category.addons.map((addon) => (
                <AddonCard key={addon.id} addon={addon} />
              ))}
            </div>
          </div>
        </section>
      ))}

      <section className="grid border-b-2 border-divider md:grid-cols-2">
        <div className="border-b-2 border-divider px-4 py-6 md:border-r-2 md:border-b-0">
          <SectionIntro index="Setup" title="How to install" />
        </div>
        <ol>
          {ADDON_INSTALL_STEPS.map((step, index) => (
            <li
              key={step}
              className="grid grid-cols-[36px_1fr] gap-2 border-b border-divider px-4 py-3 text-[15px] leading-[1.5] last:border-b-0"
            >
              <span className="font-extrabold text-accent">{String(index + 1).padStart(2, "0")}</span>
              <span>{step}</span>
            </li>
          ))}
        </ol>
      </section>
    </>
  );
}

function AddonCard({ addon }: { addon: Addon }) {
  const byline = [addon.author === null ? null : `by ${addon.author}`, addon.version && `v${addon.version}`]
    .filter(Boolean)
    .join(" · ");

  return (
    <article className="grid content-start gap-3 border-r border-b border-divider p-4">
      <div className="grid gap-1">
        <h3 className="text-[24px] leading-tight font-extrabold tracking-[-0.02em]">{addon.name}</h3>
        {byline !== "" && <p className="text-xs text-ink/60">{byline}</p>}
      </div>
      <p className="text-[15px] leading-[1.5]">{addon.summary}</p>
      {addon.features !== undefined && <Features features={addon.features} compact />}
      <DownloadButton addon={addon} />
      {addon.folders.length > 1 && (
        <p className="text-xs text-ink/60">
          Contains {addon.folders.length} folders: {addon.folders.join(", ")}. Copy all of them.
        </p>
      )}
      <div className="border-t border-divider">
        <Commands commands={addon.commands} compact />
      </div>
      {addon.credits !== undefined && <p className="text-xs text-ink/60">{addon.credits}</p>}
    </article>
  );
}

function Features({
  features,
  compact = false,
  className,
}: {
  features: readonly string[];
  compact?: boolean;
  className?: string;
}) {
  return (
    <ul className={`grid ${compact ? "gap-1.5" : "gap-2"} ${className ?? ""}`}>
      {features.map((feature) => (
        <li
          key={feature}
          className={`grid grid-cols-[16px_1fr] gap-2 ${compact ? "text-sm leading-[1.45]" : "text-[15px] leading-[1.5]"}`}
        >
          <span className={`${compact ? "mt-[6px]" : "mt-[7px]"} size-2 bg-accent`} aria-hidden="true" />
          {feature}
        </li>
      ))}
    </ul>
  );
}

function DownloadButton({ addon, large = false }: { addon: Addon; large?: boolean }) {
  const file = addon.download.split("/").pop();
  return (
    <a
      href={addon.download}
      download
      className={
        large
          ? "inline-flex w-fit items-center gap-2 bg-accent px-4 py-2.5 text-base font-extrabold text-surface transition-[filter] hover:brightness-110"
          : "inline-flex w-fit items-center gap-2 bg-accent px-3 py-1.5 text-sm font-extrabold text-surface transition-[filter] hover:brightness-110"
      }
    >
      <svg viewBox="0 0 16 16" aria-hidden="true" className="size-4">
        <path d="M8 1v9m0 0L4 6m4 4l4-4M2 13h12" fill="none" stroke="currentColor" strokeWidth="2" />
      </svg>
      Download {file}
    </a>
  );
}

function Commands({ commands, compact = false }: { commands: readonly AddonCommand[]; compact?: boolean }) {
  return (
    <dl>
      {commands.map(({ command, description }) => (
        <div
          key={command}
          className={
            compact
              ? "grid gap-0.5 border-b border-divider py-2 last:border-b-0"
              : "grid gap-1 border-b border-divider px-4 py-3 last:border-b-0"
          }
        >
          <dt>
            <code className="bg-surface-raised px-1.5 py-0.5 font-mono text-[13px] text-label">{command}</code>
          </dt>
          <dd className="text-sm text-ink/80">{description}</dd>
        </div>
      ))}
    </dl>
  );
}
