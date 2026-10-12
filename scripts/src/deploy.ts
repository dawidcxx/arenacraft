#!/usr/bin/env bun
/**
 * deploy - build the core image for the arm server and load it on the target's
 * local container engine. No registry: the thin multi-stage image is streamed
 * over ssh as `save | gzip | ssh ... 'gunzip | load'`, and only that final image
 * is sent (zig/build caches live in build mounts).
 *
 * Build + ship only. The running services pick the image up on their next
 * restart, so run ./scripts/restart_world afterwards.
 *
 * Works with podman or docker (auto-detected). Native arm64 builds directly; a
 * cross-arch build on Linux falls back to binfmt/qemu emulation, slow but cached.
 *
 *   ./scripts/deploy
 *
 * ARENACRAFT_HOST overrides the ssh target (default arenacraft@hetznerbox).
 */
import { $ } from "bun";
import { existsSync } from "node:fs";
import { join } from "node:path";
import { repoRoot } from "./lib/repo.ts";

const target = process.env.ARENACRAFT_HOST ?? "arenacraft@hetznerbox";
const image = "arenacraft:local";
const platform = "linux/arm64";
const remoteEngine = "podman";
const fmt = "{{.Architecture}}";

const dockerfile = join(repoRoot, "Dockerfile");
const targetArch = (platform.split("/")[1] ?? platform).trim();
const crossArch = targetArch !== hostArch();

function fail(message: string): never {
  console.error(message);
  process.exit(1);
}

/** surface a ShellError's exit code instead of a stack trace */
async function shell(label: string, run: () => Promise<unknown>): Promise<void> {
  try {
    await run();
  } catch (err) {
    const code = (err as { exitCode?: number }).exitCode ?? 1;
    console.error(`\n${label} failed (exit ${code})`);
    process.exit(code);
  }
}

async function capture(label: string, run: () => Promise<string>): Promise<string> {
  try {
    return (await run()).trim();
  } catch (err) {
    const code = (err as { exitCode?: number }).exitCode ?? 1;
    console.error(`\n${label} failed (exit ${code})`);
    process.exit(code);
  }
}

function hostArch(): string {
  switch (process.arch) {
    case "x64":
      return "amd64";
    case "arm64":
      return "arm64";
    case "ia32":
      return "386";
    default:
      return process.arch;
  }
}

const BINFMT_NAME: Record<string, string> = {
  arm64: "aarch64-linux",
  amd64: "x86_64-linux",
  arm: "arm-linux",
  "386": "i386-linux",
  riscv64: "riscv64-linux",
  ppc64le: "ppc64le-linux",
  s390x: "s390x-linux",
};

/**
 * NixOS registers qemu binfmt without the F (fix binary) flag, so the
 * interpreter is resolved inside the container. Bind-mounting it (and the
 * /nix/store shim target) lets emulated RUN steps exec. Podman-only; docker
 * buildx brings its own emulation. No-op when the target is the host arch.
 */
async function qemuMounts(): Promise<string[]> {
  if (!crossArch) return [];
  const name = BINFMT_NAME[targetArch];
  if (!name) return [];
  const entry = `/proc/sys/fs/binfmt_misc/${name}`;
  if (!existsSync(entry)) return [];
  if (/flags:.*F/.test(await Bun.file(entry).text())) return [];
  const mounts: string[] = [];
  if (existsSync("/run/binfmt")) mounts.push("-v", "/run/binfmt:/run/binfmt:ro");
  if (existsSync("/nix/store")) mounts.push("-v", "/nix/store:/nix/store:ro");
  if (mounts.length > 0) console.log(`note: binfmt '${name}' has no F flag; bind-mounting its interpreter`);
  return mounts;
}

const engine = Bun.which("podman") ? "podman" : Bun.which("docker") ? "docker" : fail("neither podman nor docker found in PATH");

if (!Bun.which("ssh")) fail("ssh not found in PATH");
if (!existsSync(dockerfile)) fail(`Dockerfile not found: ${dockerfile}`);

// only podman needs the host bind-mounts; docker buildx embeds its own qemu
const mounts = engine === "podman" ? await qemuMounts() : [];
// docker saves a docker-archive by default; podman needs it spelled out
const saveArgs = engine === "podman" ? ["--format", "docker-archive"] : [];
const remoteLoad = `gunzip | ${remoteEngine} load`;

const note = crossArch ? " (emulated; first run is slow)" : "";
console.log(`building ${image} for ${platform} with ${engine}${note}`);
await shell(`${engine} build`, () =>
  $`${engine} build --platform ${platform} ${mounts} -f ${dockerfile} -t ${image} ${repoRoot}`,
);

const arch = await capture(`${engine} image inspect`, () =>
  $`${engine} image inspect --format ${fmt} ${image}`.text(),
);
if (arch !== targetArch) fail(`refusing to ship: ${image} is ${arch}, want ${targetArch}`);
console.log(`built ${image} (${arch})`);

console.log(`shipping ${image} -> ${target} (${remoteEngine})`);
await shell("ship", () => $`${engine} save ${saveArgs} ${image} | gzip -1 | ssh ${target} ${remoteLoad}`);

const remoteArch = await capture("remote inspect", () =>
  $`ssh ${target} ${remoteEngine} image inspect --format ${fmt} ${image}`.text(),
);
if (remoteArch !== targetArch) fail(`target has the wrong arch: ${remoteArch}, want ${targetArch}`);
console.log(`loaded on ${target} (${remoteArch})`);

console.log(`\nnext: ./scripts/restart_world`);
