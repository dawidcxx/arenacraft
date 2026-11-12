import { Link, NavLink } from "react-router-dom";
import { DiscordIcon } from "@/components/DiscordIcon";
import { cn } from "@/lib/cn";
import { DISCORD_URL, NAV_LINKS } from "@/lib/site";

export function SiteHeader() {
  return (
    <header className="sticky top-0 z-50 border-b-2 border-divider bg-surface">
      <nav className="flex items-center gap-2 px-4 py-3">
        <Link to="/" className="flex items-center gap-2 text-sm font-extrabold">
          <span className="size-3.5 bg-accent" aria-hidden="true" />
          Arenacraft
        </Link>

        <div className="ml-auto flex items-center gap-1">
          <div className="hidden items-center md:flex">
            {NAV_LINKS.map((link) => (
              <NavLink
                key={link.to}
                to={link.to}
                end={link.to === "/"}
                className={({ isActive }) =>
                  cn(
                    "border-b-2 px-3.5 py-2.5 text-sm leading-[22px] font-bold transition-colors",
                    isActive
                      ? "border-accent text-ink"
                      : "border-transparent text-ink/65 hover:text-ink",
                  )
                }
              >
                {link.label}
              </NavLink>
            ))}
          </div>

          <a
            href={DISCORD_URL}
            className="inline-flex items-center gap-2.5 bg-discord px-3.5 py-2 text-sm font-extrabold text-white transition-[filter] hover:brightness-110"
          >
            <DiscordIcon className="size-5" />
            Discord
          </a>
        </div>
      </nav>
    </header>
  );
}