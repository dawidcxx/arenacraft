import type { Countdown } from "@/hooks/useCountdown";

export function formatCountdown({ days, hours, minutes }: Countdown): string {
  if (days > 0) return `${days}d ${hours}h ${minutes}m`;
  if (hours > 0) return `${hours}h ${minutes}m`;
  return `${minutes}m`;
}