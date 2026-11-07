#!/usr/bin/env bun
/**
 * restart_world - restart the world service on the server and clean up stale
 * core images.
 *
 * ssh into the target, restart the systemd-managed world container so it picks
 * up the image ./scripts/deploy loaded, then prune the images left behind by
 * repeated deploys. Both commands run as the `arenacraft` user over ssh, so the
 * prune hits the same rootless podman store that `deploy` loads into.
 *
 *   ./scripts/restart_world          # prod world (arenacraft:prod)
 *   ./scripts/restart_world --ptr    # PTR world (arenacraft:ptr)
 *
 * ARENACRAFT_HOST overrides the ssh target (default arenacraft@netcupbox).
 */
import { $ } from "bun";
import { Command } from "commander";

const program = new Command();
program
  .name("restart_world")
  .description("Restart the world service on the server and prune stale images.")
  .option("--ptr", "restart the PTR world service instead of prod")
  .parse(process.argv);

const { ptr } = program.opts<{ ptr: boolean }>();

const target = process.env.ARENACRAFT_HOST ?? "arenacraft@netcupbox";
const service = ptr ? "podman-arenacraft-world-ptr" : "podman-arenacraft-world";

console.log(`restarting ${service} on ${target}`);
try {
  await $`ssh ${target} sudo systemctl restart ${service}`;
} catch (err) {
  const code = (err as { exitCode?: number }).exitCode ?? 1;
  console.error(`\nrestart failed (exit ${code})`);
  process.exit(code);
}

// The restart is the point; a failed prune should not mask its success.
console.log(`pruning unused podman images on ${target}`);
try {
  await $`ssh ${target} podman image prune -a -f`;
} catch (err) {
  const code = (err as { exitCode?: number }).exitCode ?? 1;
  console.error(`warning: image prune failed (exit ${code})`);
}
