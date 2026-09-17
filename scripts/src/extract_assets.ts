#!/usr/bin/env bun
/**
 * extract_assets - populates data/{dbc,maps,vmaps} from a game client
 * directory using the extractor binaries shipped with `ac`.
 *
 * Pipeline (see OLD_SCRIPTS.md, extract-client is the reference):
 *   1. map_extractor    -> dbc, maps   (accepts -i/-o)
 *   2. vmap4_extractor  -> Buildings, vmaps (reads Data/ from cwd)
 *   3. vmap4_assembler  -> vmaps (consolidates Buildings)
 *   4. mmaps_generator  -> mmaps (slow, opt-in via --with-mmaps)
 *
 * The vmap tools are cwd-relative, so everything runs in a temp workdir with
 * the client's Data/ directory symlinked in; results are moved into the
 * destination afterwards and the client directory is left untouched.
 */
import { Command } from "commander";
import { join, resolve } from "node:path";
import { cpSync, existsSync, mkdtempSync, renameSync, rmSync, symlinkSync } from "node:fs";
import { tmpdir } from "node:os";
import { repoRoot } from "./lib/repo.ts";

const program = new Command();
program
  .name("extract_assets")
  .description("Extract dbc/maps/vmaps (and optionally mmaps) from a game client via the ac extractor binaries.")
  .option("--client-dir <dir>", "game client directory (must contain Data/)", join(process.env.HOME ?? "~", "Games/wotlk-dev"))
  .option("--dest <dir>", "output directory for the extracted data", join(repoRoot, "data"))
  .option("--bin <path>", "path to the ac binary", join(repoRoot, "zig-out", "bin", "ac"))
  .option("--with-mmaps", "also run the (slow) mmaps_generator", false)
  .parse(process.argv);

const opts = program.opts<{ clientDir: string; dest: string; bin: string; withMmaps: boolean }>();
const clientDir = resolve(opts.clientDir);
const dest = resolve(opts.dest);
const bin = resolve(opts.bin);

if (!existsSync(clientDir)) {
  console.error(`client dir not found: ${clientDir}`);
  process.exit(1);
}
if (!existsSync(join(clientDir, "Data"))) {
  console.error(`not a game client (no Data/ directory): ${clientDir}`);
  process.exit(1);
}
if (!existsSync(bin)) {
  console.error(`ac binary not found: ${bin} (run 'zig build ac' first)`);
  process.exit(1);
}
const workdir = mkdtempSync(join(tmpdir(), "ac-extract-"));
symlinkSync(join(clientDir, "Data"), join(workdir, "Data"), "dir");
console.log(`client:   ${clientDir}`);
console.log(`dest:     ${dest}`);
console.log(`workdir:  ${workdir}`);

type Step = { tool: string; args: string[]; outputs: string[] };
const steps: Step[] = [
  { tool: "map_extractor", args: ["-i", workdir, "-o", workdir], outputs: ["dbc", "maps"] },
  { tool: "vmap4_extractor", args: [], outputs: ["vmaps"] },
  { tool: "vmap4_assembler", args: [], outputs: [] },
  ...(opts.withMmaps ? [{ tool: "mmaps_generator", args: [], outputs: ["mmaps"] }] : []),
];

const t0 = performance.now();
for (const step of steps) {
  console.log(`\n=== ${step.tool} ===`);
  const proc = Bun.spawn([bin, step.tool, ...step.args], { cwd: workdir, stdout: "inherit", stderr: "inherit", stdin: "inherit" });
  const code = await proc.exited;
  if (code !== 0) {
    console.error(`${step.tool} failed with exit code ${code} (workdir kept for debugging: ${workdir})`);
    process.exit(code);
  }
}

console.log("\n=== moving outputs ===");
for (const dir of steps.flatMap((s) => s.outputs)) {
  const from = join(workdir, dir);
  if (!existsSync(from)) continue;
  // replace stale outputs individually - dest also holds data/sql
  rmSync(join(dest, dir), { recursive: true, force: true });
  renameSync(from, join(dest, dir));
  console.log(`data/${dir}`);
}
rmSync(workdir, { recursive: true, force: true });

console.log(`\ndone in ${Math.round((performance.now() - t0) / 1000)}s`);
console.log(`hint: the core looks these up via AC_DATADIR (exe-relative 'data' by default)`);
