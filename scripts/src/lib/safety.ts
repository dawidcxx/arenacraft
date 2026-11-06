/** Shared guards for scripts that mutate live data. */
import { stdin, stdout } from "node:process";
import { createInterface } from "node:readline/promises";

import type { DbInfo } from "./db.ts";

/** Server/database identifier safe to print (never includes the password). */
export function describeTarget(info: DbInfo): string {
  const database = info.database ? `/${info.database}` : "";
  return `${info.username}@${info.hostname}:${info.port}${database}`;
}

export interface ConfirmOptions {
  action: string;
  target: string;
  yes: boolean;
  dryRun?: boolean;
  allowDryRun?: boolean;
}

/**
 * Blocks until the operator explicitly confirms a destructive operation.
 * `--yes` skips the prompt; `--dry-run` always wins. Anything other than an
 * explicit confirmation aborts.
 */
export async function confirmDestructive(opts: ConfirmOptions): Promise<"apply" | "dry-run"> {
  if (opts.dryRun) return "dry-run";
  if (opts.yes) return "apply";

  if (!stdin.isTTY) {
    console.error("refusing to continue without confirmation: no interactive terminal (pass --yes to override)");
    process.exit(1);
  }

  const choices = opts.allowDryRun
    ? "type 'yes' to proceed, 'dry-run' to simulate, anything else to abort"
    : "type 'yes' to proceed, anything else to abort";
  const rl = createInterface({ input: stdin, output: stdout });
  try {
    const answer = (await rl.question(`${opts.action} ${opts.target}\n${choices}: `)).trim().toLowerCase();
    if (answer === "yes" || answer === "y") return "apply";
    if (opts.allowDryRun && (answer === "dry-run" || answer === "dry" || answer === "d")) return "dry-run";
    console.error("aborted");
    process.exit(1);
  } finally {
    rl.close();
  }
}
