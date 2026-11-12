import { SectionIntro } from "@/components/SectionIntro";
import { useCopyToClipboard } from "@/hooks/useCopyToClipboard";
import { REALMLIST_COMMAND } from "@/lib/site";

export function HowToPlaySection() {
  return (
    <section className="grid border-b-2 border-divider md:grid-cols-2">
      <div className="border-b-2 border-divider px-4 py-6 md:border-r-2 md:border-b-0">
        <SectionIntro index="03 · Start" title="How to play" />
      </div>

      <div className="grid auto-rows-fr">
        <div className="border-b-2 border-divider p-4">
          <div className="mb-2 text-xs tracking-[0.1em] uppercase">Registration</div>
          <ol className="list-decimal pl-5 text-base leading-[1.45]">
            <li>Go to the bot channel</li>
            <li>Type in /register</li>
            <li>The bot should send your credentials over DM</li>
          </ol>
        </div>

        <div className="grid gap-2 p-4">
          <div className="text-xs tracking-[0.1em] uppercase">Realmlist</div>
          <div className="flex items-stretch border-2 border-divider">
            <code className="flex-1 bg-surface-raised px-4 py-3.5 font-mono text-[17px] leading-[1.55]">
              {REALMLIST_COMMAND}
            </code>
            <CopyButton />
          </div>
          <div className="text-sm">(Located in Data/enUS/realmlist.wtf)</div>
        </div>
      </div>
    </section>
  );
}

function CopyButton() {
  const { copied, copy } = useCopyToClipboard(REALMLIST_COMMAND);

  return (
    <button
      type="button"
      onClick={copy}
      className="flex shrink-0 items-center bg-accent px-3.5 py-2 text-sm font-extrabold text-surface transition-[filter] hover:brightness-110"
    >
      {copied ? "Copied" : "Copy"}
    </button>
  );
}