#!/usr/bin/env bun
/**
 * extract_assets - populates data/maps, data/vmaps, data/mmaps (+ dbc) from a
 * game client directory using the extractor binaries shipped with `ac`.
 *
 * Planned pipeline (see OLD_SCRIPTS.md, extract-client is the reference):
 *   1. map_extractor    -> maps, dbc
 *   2. vmap4_extractor  -> raw vmaps
 *   3. vmap4_assembler  -> vmaps
 *   4. mmaps_generator  -> mmaps (slow, opt-in)
 *
 * Not implemented yet - this is the skeleton.
 */
import { Command } from "commander";
import { join, resolve } from "node:path";
import { existsSync } from "node:fs";
import { repoRoot } from "./lib/repo.ts";

const program = new Command();
program
  .name("extract_assets")
  .description("Extract maps/vmaps/mmaps from a game client via the ac extractor binaries.")
  .option("--client-dir <dir>", "game client directory [~/.local/var/wow]", resolve(process.env.HOME ?? "~", ".local/var/wow"))
  .option("--dest <dir>", "output directory for the extracted data", join(repoRoot, "data"))
  .option("--bin <path>", "path to the ac binary", join(repoRoot, "zig-out", "bin", "ac"))
  .option("--with-mmaps", "also run the (slow) mmaps_generator", false)
  .parse(process.argv);

const opts = program.opts<{ clientDir: string; dest: string; bin: string; withMmaps: boolean }>();

const steps = ["map_extractor", "vmap4_extractor", "vmap4_assembler", ...(opts.withMmaps ? ["mmaps_generator"] : [])];
console.log(`client dir: ${opts.clientDir}`);
console.log(`dest dir:   ${opts.dest}`);
console.log(`ac binary:  ${opts.bin}`);
console.log("pipeline:");
for (const step of steps) console.log(`  - ${step}`);

for (const dir of [opts.clientDir, opts.dest, opts.bin]) {
  if (!existsSync(dir)) {
    console.error(`not found: ${dir}`);
    process.exit(1);
  }
}

console.error("extract_assets is not implemented yet");
process.exit(2);
