import type { ReactNode } from "react";
import { cn } from "@/lib/cn";

const TONES = {
  live: "bg-accent-soft text-accent-deep",
  outline: "border border-accent text-accent",
  neutral: "bg-[#f8f4f4] text-[#444141]",
} as const;

export type TagTone = keyof typeof TONES;

export function Tag({ children, tone = "live" }: { children: ReactNode; tone?: TagTone }) {
  return (
    <span
      className={cn(
        "inline-flex w-fit items-center px-2.5 py-[3px] text-[11px] leading-[1.55] tracking-[0.02em]",
        TONES[tone],
      )}
    >
      {children}
    </span>
  );
}