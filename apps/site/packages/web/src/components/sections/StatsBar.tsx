import { cn } from "@/lib/cn";

const STATS = [
  { value: "264", label: "ilvl gear" },
  { value: "277", label: "ilvl weapons" },
  { value: "3v3", label: "Melee / Caster / Healer" },
  { value: "3.3.5", label: "WotLK client" },
] as const;

export function StatsBar() {
  return (
    <div className="grid grid-cols-2 border-b-2 border-divider md:grid-cols-4">
      {STATS.map((stat, index) => (
        <div
          key={stat.value}
          className={cn(
            "p-4",
            index % 2 === 0 && "border-r-2 border-divider",
            index < 2 && "border-b-2 border-divider md:border-b-0",
            index !== 3 && "md:border-r-2 md:border-divider",
          )}
        >
          <div className="text-[44px] leading-[1.55] font-extrabold tracking-[-0.03em]">
            {stat.value}
          </div>
          <div className="text-[13px] leading-[1.55]">{stat.label}</div>
        </div>
      ))}
    </div>
  );
}