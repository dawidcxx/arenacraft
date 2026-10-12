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
 *   ./scripts/restart_world
 *
 * ARENACRAFT_HOST overrides the ssh target (default arenacraft@hetznerbox).
 */
import { $ } from "bun";

const target = process.env.ARENACRAFT_HOST ?? "arenacraft@hetznerbox";

console.log(`restarting podman-arenacraft-world on ${target}`);
try {
  await $`ssh ${target} sudo systemctl restart podman-arenacraft-world`;
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
