#!/usr/bin/env bun
/**
 * deploy - build the core image for the arm server and load it on the target's
 * local container engine. No registry: the thin multi-stage image is streamed
 * over ssh as `save | gzip | ssh ... 'gunzip | load'`, and only that final image
 * is sent (zig/build caches live in build mounts).
 *
 * Works with podman or docker (auto-detected, override with --engine). Native
 * arm64 (e.g. an Apple Silicon mac) builds directly; a cross-arch build on
 * Linux falls back to binfmt/qemu emulation, which is slow but cached.
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
  remoteEngine: "podman" as const, // the target runs podman + podman compose
};

const ENGINES = ["podman", "docker"] as const;
type Engine = (typeof ENGINES)[number];

interface Options {
  target: string;
  image: string;
  platform: string;
  engine: string | undefined;
  remoteEngine: string;
  skipBuild: boolean;
  ship: boolean;
  up: boolean;
  remoteDir: string | undefined;
  dryRun: boolean;
}

const program = new Command();
program
  .name("deploy")
  .description("Build the core image and load it on the target's container engine (podman/docker).")
  .option("--target <user@host>", "ssh target", DEFAULTS.target)
  .option("--image <ref>", "image to build and load", DEFAULTS.image)
  .option("--platform <os/arch>", "target platform", DEFAULTS.platform)
  .option("--engine <podman|docker>", "local engine (default: podman, else docker)")
  .option("--remote-engine <podman|docker>", "engine on the target", DEFAULTS.remoteEngine)
  .option("--skip-build", "reuse the local image", false)
  .option("--no-ship", "build and verify only, no transfer")
  .option("--up", "also run 'up -d --no-build' via the target's compose", false)
  .option("--remote-dir <dir>", "target dir holding docker-compose.yml (for --up)")
  .option("--dry-run", "print commands without running them", false)
  .parse(process.argv);

const opts = program.opts<Options>();
const dockerfile = join(repoRoot, "Dockerfile");
const fmt = "{{.Architecture}}";
const targetArch = (opts.platform.split("/")[1] ?? opts.platform).trim();
const crossArch = targetArch !== hostArch();

function fail(message: string): never {
  console.error(message);
  process.exit(1);
}

function quote(s: string): string {
  return `'${s.replaceAll("'", `'\\''`)}'`;
}

function asEngine(value: string, flag: string): Engine {
  if (!(ENGINES as readonly string[]).includes(value)) {
    fail(`${flag} must be one of: ${ENGINES.join(", ")} (got '${value}')`);
  }
  return value as Engine;
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

const engine: Engine = opts.engine
  ? asEngine(opts.engine, "--engine")
  : Bun.which("podman")
    ? "podman"
    : Bun.which("docker")
      ? "docker"
      : fail("neither podman nor docker found in PATH");
const remoteEngine = asEngine(opts.remoteEngine, "--remote-engine");

if (opts.ship && !Bun.which("ssh")) fail("ssh not found in PATH");
if (opts.up && !opts.remoteDir) fail("--up requires --remote-dir <dir>");
if (!existsSync(dockerfile)) fail(`Dockerfile not found: ${dockerfile}`);

// only podman needs the host bind-mounts; docker buildx embeds its own qemu
const mounts = engine === "podman" ? await qemuMounts() : [];
// docker saves a docker-archive by default; podman needs it spelled out
const saveArgs = engine === "podman" ? ["--format", "docker-archive"] : [];
const remoteLoad = `gunzip | ${remoteEngine} load`;

if (!opts.skipBuild) {
  const note = crossArch ? " (emulated; first run is slow)" : "";
  console.log(`building ${opts.image} for ${opts.platform} with ${engine}${note}`);
  if (opts.dryRun) {
    console.log(`+ ${engine} build --platform ${opts.platform} ${mounts.map(quote).join(" ")} -f ${quote(dockerfile)} -t ${quote(opts.image)} ${quote(repoRoot)}`);
  } else {
    await shell(`${engine} build`, () =>
      $`${engine} build --platform ${opts.platform} ${mounts} -f ${dockerfile} -t ${opts.image} ${repoRoot}`,
    );
  }
} else {
  console.log(`skipping build, reusing ${opts.image}`);
}

if (opts.dryRun) {
  console.log(`+ ${engine} image inspect --format ${quote(fmt)} ${quote(opts.image)}`);
} else {
  const arch = await capture(`${engine} image inspect`, () =>
    $`${engine} image inspect --format ${fmt} ${opts.image}`.text(),
  );
  if (arch !== targetArch) fail(`refusing to ship: ${opts.image} is ${arch}, want ${targetArch}`);
  console.log(`built ${opts.image} (${arch})`);
}

if (opts.ship) {
  console.log(`shipping ${opts.image} -> ${opts.target} (${remoteEngine})`);
  if (opts.dryRun) {
    console.log(`+ ${engine} save ${saveArgs.join(" ")} ${quote(opts.image)} | gzip -1 | ssh ${quote(opts.target)} ${quote(remoteLoad)}`);
  } else {
    await shell("ship", () =>
      $`${engine} save ${saveArgs} ${opts.image} | gzip -1 | ssh ${opts.target} ${remoteLoad}`,
    );
    const arch = await capture("remote inspect", () =>
      $`ssh ${opts.target} ${remoteEngine} image inspect --format ${fmt} ${opts.image}`.text(),
    );
    if (arch !== targetArch) fail(`target has the wrong arch: ${arch}, want ${targetArch}`);
    console.log(`loaded on ${opts.target} (${arch})`);
  }
}

if (opts.up && opts.remoteDir) {
  console.log(`restarting on ${opts.target} in ${opts.remoteDir}`);
  const remoteUp = `cd ${opts.remoteDir} && ${remoteEngine} compose up -d --no-build`;
  if (opts.dryRun) {
    console.log(`+ ssh ${quote(opts.target)} ${quote(remoteUp)}`);
  } else {
    await shell("compose up", () => $`ssh ${opts.target} ${remoteUp}`);
  }
} else if (opts.ship && !opts.dryRun) {
  console.log(`\nnext: ssh ${opts.target} '${remoteEngine} compose up -d --no-build'   # from the docker-compose.yml dir`);
}
