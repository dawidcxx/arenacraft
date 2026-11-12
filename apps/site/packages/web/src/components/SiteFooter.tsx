import { REALM_HOST } from "@/lib/site";

export function SiteFooter() {
  return (
    <footer className="flex flex-wrap justify-between gap-2 px-4 py-3 text-xs">
      <span>
        Arenacraft — private 3.3.5a arena server. Not affiliated with Blizzard Entertainment.
      </span>
      <span>{REALM_HOST}</span>
    </footer>
  );
}