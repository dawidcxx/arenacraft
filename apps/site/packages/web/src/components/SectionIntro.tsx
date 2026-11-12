import type { ReactNode } from "react";
import { cn } from "@/lib/cn";

export function SectionIntro({
  index,
  title,
  className,
}: {
  index: string;
  title: ReactNode;
  className?: string;
}) {
  return (
    <div className={cn("grid content-start gap-3", className)}>
      <p className="text-xs tracking-[0.1em] text-label uppercase">{index}</p>
      <h2 className="text-balance text-[clamp(2rem,5.5vw,2.75rem)] leading-none font-extrabold tracking-[-0.03em]">
        {title}
      </h2>
    </div>
  );
}