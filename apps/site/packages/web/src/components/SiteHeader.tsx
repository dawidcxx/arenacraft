import { useEffect, useId, useRef, useState } from "react";
import { Link, NavLink, useLocation } from "react-router-dom";
import { DiscordIcon } from "@/components/DiscordIcon";
import { cn } from "@/lib/cn";
import { DISCORD_URL, NAV_LINKS, NAV_MORE_LINKS } from "@/lib/site";

const NAV_ITEM =
  "border-b-2 px-2.5 py-2.5 text-sm leading-[22px] font-bold transition-colors sm:px-3.5";

export function SiteHeader() {
  return (
    <header className="sticky top-0 z-50 border-b-2 border-divider bg-surface">
      <nav className="flex items-center gap-2 px-4 py-3">
        <Link to="/" className="flex items-center gap-2 text-sm font-extrabold">
          <span className="size-3.5 bg-accent" aria-hidden="true" />
          <span className="hidden min-[420px]:inline">Arenacraft</span>
        </Link>

        <div className="ml-auto flex items-center gap-1">
          {NAV_LINKS.map((link) => (
            <NavLink
              key={link.to}
              to={link.to}
              end={link.to === "/"}
              className={({ isActive }) =>
                cn(
                  NAV_ITEM,
                  isActive ? "border-accent text-ink" : "border-transparent text-ink/65 hover:text-ink",
                )
              }
            >
              {link.label}
            </NavLink>
          ))}

          <MoreMenu />

          <a
            href={DISCORD_URL}
            aria-label="Discord"
            className="ml-1 inline-flex items-center gap-2.5 bg-discord px-3 py-2 text-sm font-extrabold text-white transition-[filter] hover:brightness-110 sm:px-3.5"
          >
            <DiscordIcon className="size-5" />
            <span className="hidden sm:inline">Discord</span>
          </a>
        </div>
      </nav>
    </header>
  );
}

function MoreMenu() {
  const [open, setOpen] = useState(false);
  const rootRef = useRef<HTMLDivElement>(null);
  const menuId = useId();
  const { pathname } = useLocation();
  const activeInMenu = NAV_MORE_LINKS.some(
    (link) => pathname === link.to || pathname.startsWith(`${link.to}/`),
  );

  // Close after navigating, on Escape and on a click anywhere else.
  useEffect(() => setOpen(false), [pathname]);
  useEffect(() => {
    if (!open) return;
    const onPointerDown = (event: PointerEvent) => {
      if (!rootRef.current?.contains(event.target as Node)) setOpen(false);
    };
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === "Escape") setOpen(false);
    };
    document.addEventListener("pointerdown", onPointerDown);
    document.addEventListener("keydown", onKeyDown);
    return () => {
      document.removeEventListener("pointerdown", onPointerDown);
      document.removeEventListener("keydown", onKeyDown);
    };
  }, [open]);

  return (
    <div ref={rootRef} className="relative">
      <button
        type="button"
        aria-expanded={open}
        aria-controls={menuId}
        onClick={() => setOpen((value) => !value)}
        className={cn(
          NAV_ITEM,
          "inline-flex items-center gap-1.5",
          activeInMenu || open ? "border-accent text-ink" : "border-transparent text-ink/65 hover:text-ink",
        )}
      >
        More
        <svg
          viewBox="0 0 10 6"
          aria-hidden="true"
          className={cn("size-2.5 transition-transform", open && "rotate-180")}
        >
          <path d="M1 1l4 4 4-4" fill="none" stroke="currentColor" strokeWidth="1.8" />
        </svg>
      </button>

      {open && (
        <ul
          id={menuId}
          className="absolute right-0 mt-[14px] min-w-48 border-2 border-divider bg-surface shadow-[0_12px_32px_rgba(0,0,0,0.6)]"
        >
          {NAV_MORE_LINKS.map((link) => (
            <li key={link.to} className="border-b border-divider last:border-b-0">
              <NavLink
                to={link.to}
                className={({ isActive }) =>
                  cn(
                    "flex items-center gap-2 border-l-2 px-4 py-2.5 text-sm font-bold transition-colors",
                    isActive
                      ? "border-accent bg-surface-raised text-ink"
                      : "border-transparent text-ink/75 hover:bg-surface-raised hover:text-ink",
                  )
                }
              >
                {link.label}
              </NavLink>
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}
