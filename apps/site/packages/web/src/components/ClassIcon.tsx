import deathKnightIcon from "@/assets/classes/death-knight.svg";
import druidIcon from "@/assets/classes/druid.svg";
import hunterIcon from "@/assets/classes/hunter.svg";
import mageIcon from "@/assets/classes/mage.svg";
import paladinIcon from "@/assets/classes/paladin.svg";
import priestIcon from "@/assets/classes/priest.svg";
import rogueIcon from "@/assets/classes/rogue.svg";
import shamanIcon from "@/assets/classes/shaman.svg";
import warlockIcon from "@/assets/classes/warlock.svg";
import warriorIcon from "@/assets/classes/warrior.svg";
import { cn } from "@/lib/cn";
import { CLASS_COLORS } from "@/lib/ladder";

// Classic-era icon set (brutaliccus/ClassicWoWClassIcons_Circle_HighRes). It has no
// Death Knight, so that one is a VTracer vectorization of the WotLK class icon.
// The rest are VTracer vectorizations of the original raster icons as well (see
// scripts/vectorize-class-icons), all clipped to the same circular frame.
const CLASS_ICONS: Record<string, string> = {
  "Death Knight": deathKnightIcon,
  Druid: druidIcon,
  Hunter: hunterIcon,
  Mage: mageIcon,
  Paladin: paladinIcon,
  Priest: priestIcon,
  Rogue: rogueIcon,
  Shaman: shamanIcon,
  Warlock: warlockIcon,
  Warrior: warriorIcon,
};

export function ClassIcon({
  wowClass,
  className = "size-6",
}: {
  wowClass: string;
  className?: string;
}) {
  const icon = CLASS_ICONS[wowClass];

  if (icon) {
    return (
      <img src={icon} alt="" width={24} height={24} className={cn("rounded-full", className)} />
    );
  }

  return (
    <span
      aria-hidden="true"
      className={cn(
        "grid place-items-center rounded-full text-[9px] font-extrabold text-white ring-2 ring-black/70",
        className,
      )}
      style={{ backgroundColor: CLASS_COLORS[wowClass] ?? "#444141" }}
    >
      {initials(wowClass)}
    </span>
  );
}

function initials(wowClass: string): string {
  return wowClass
    .split(" ")
    .map((word) => word[0])
    .join("")
    .slice(0, 2);
}