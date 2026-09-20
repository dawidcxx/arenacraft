#!/usr/bin/env bun
/**
 * deploy - build the core image for the arm server here and load it on the
 * target's local podman. No registry: the thin multi-stage image is streamed
 * over ssh as `podman save | gzip | ssh ... 'gunzip | podman load'`, and only
 * that final image is sent (zig/build caches live in build mounts).
 *
 * The build runs under binfmt/qemu emulation (linux/arm64 by default), which is
 * slow the first time; the caches persist, so later builds are incremental.
 *
 *   scripts/deploy                                  # build + ship (the normal one)
 *   scripts/deploy --skip-build                     # ship what's already here
 *   scripts/deploy --no-ship                        # build + verify arch only
 *   scripts/deploy --up --remote-dir ~/arenacraft   # also restart the stack there
 */
import { $ } from "bun";
import { Command } from "commander";
import { existsSync } from "node:fs";
import { join } from "node:path";
import { repoRoot } from "./lib/repo.ts";

const DEFAULTS = {
  target: "dawid@hetznerbox",
  image: "arenacraft:local",
  platform: "linux/arm64",
};

interface Options {
  target: string;
  image: string;
  platform: string;
  skipBuild: boolean;
  ship: boolean;
  up: boolean;
  remoteDir: string | undefined;
  dryRun: boolean;
}

const program = new Command();
program
  .name("deploy")
  .description("Build the core image here (qemu-emulated) and load it on the target's podman.")
  .option("--target <user@host>", "ssh target", DEFAULTS.target)
  .option("--image <ref>", "image to build and load", DEFAULTS.image)
  .option("--platform <os/arch>", "target platform", DEFAULTS.platform)
  .option("--skip-build", "reuse the local image", false)
  .option("--no-ship", "build and verify only, no transfer")
  .option("--up", "also run 'podman compose up -d --no-build' on the target", false)
  .option("--remote-dir <dir>", "target dir holding docker-compose.yml (for --up)")
  .option("--dry-run", "print commands without running them", false)
  .parse(process.argv);

const opts = program.opts<Options>();
const dockerfile = join(repoRoot, "Dockerfile");
const fmt = "{{.Architecture}}";
const targetArch = (opts.platform.split("/")[1] ?? opts.platform).trim();

function fail(message: string): never {
  console.error(message);
  process.exit(1);
}

function quote(s: string): string {
  return `'${s.replaceAll("'", `'\\''`)}'`;
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
 * /nix/store shim target) lets emulated RUN steps exec. No-op everywhere else.
 */
async function qemuMounts(): Promise<string[]> {
  if (targetArch === hostArch()) return [];
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

if (!Bun.which("podman")) fail("podman not found in PATH");
if (opts.ship && !Bun.which("ssh")) fail("ssh not found in PATH");
if (opts.up && !opts.remoteDir) fail("--up requires --remote-dir <dir>");
if (!existsSync(dockerfile)) fail(`Dockerfile not found: ${dockerfile}`);

const mounts = await qemuMounts();

if (!opts.skipBuild) {
  console.log(`building ${opts.image} for ${opts.platform} (emulated; first run is slow)`);
  if (opts.dryRun) {
    console.log(`+ podman build --platform ${opts.platform} ${mounts.map(quote).join(" ")} -f ${quote(dockerfile)} -t ${quote(opts.image)} ${quote(repoRoot)}`);
  } else {
    await shell("podman build", () =>
      $`podman build --platform ${opts.platform} ${mounts} -f ${dockerfile} -t ${opts.image} ${repoRoot}`,
    );
  }
} else {
  console.log(`skipping build, reusing ${opts.image}`);
}

if (opts.dryRun) {
  console.log(`+ podman image inspect --format ${quote(fmt)} ${quote(opts.image)}`);
} else {
  const arch = await capture("podman image inspect", () =>
    $`podman image inspect --format ${fmt} ${opts.image}`.text(),
  );
  if (arch !== targetArch) fail(`refusing to ship: ${opts.image} is ${arch}, want ${targetArch}`);
  console.log(`built ${opts.image} (${arch})`);
}

if (opts.ship) {
  console.log(`shipping ${opts.image} -> ${opts.target}`);
  const remoteLoad = "gunzip | podman load";
  if (opts.dryRun) {
    console.log(`+ podman save --format docker-archive ${quote(opts.image)} | gzip -1 | ssh ${quote(opts.target)} ${quote(remoteLoad)}`);
  } else {
    await shell("ship", () =>
      $`podman save --format docker-archive ${opts.image} | gzip -1 | ssh ${opts.target} ${remoteLoad}`,
    );
    const arch = await capture("remote inspect", () =>
      $`ssh ${opts.target} podman image inspect --format ${fmt} ${opts.image}`.text(),
    );
    if (arch !== targetArch) fail(`target has the wrong arch: ${arch}, want ${targetArch}`);
    console.log(`loaded on ${opts.target} (${arch})`);
  }
}

if (opts.up && opts.remoteDir) {
  console.log(`restarting on ${opts.target} in ${opts.remoteDir}`);
  const remoteUp = `cd ${opts.remoteDir} && podman compose up -d --no-build`;
  if (opts.dryRun) {
    console.log(`+ ssh ${quote(opts.target)} ${quote(remoteUp)}`);
  } else {
    await shell("compose up", () => $`ssh ${opts.target} ${remoteUp}`);
  }
} else if (opts.ship && !opts.dryRun) {
  console.log(`\nnext: ssh ${opts.target} 'podman compose up -d --no-build'   # from the docker-compose.yml dir`);
}
