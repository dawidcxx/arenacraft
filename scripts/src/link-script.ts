#!/usr/bin/env bun

import { $ } from "bun";
import { requireProjectDir } from "./shared";
import { readdir, symlink, stat, unlink } from "fs/promises";
import { join, basename, extname, resolve } from "path";
import prompts from "prompts";
import { realpath } from "fs/promises";

await requireProjectDir();

const srcDir = "./scripts/src";
const binDir = "./scripts/bin";

// Get all .ts files in srcDir
const files = (await readdir(srcDir)).filter((f) => extname(f) === ".ts");

// Prompt user to select a file
const { file } = await prompts({
  type: "select",
  name: "file",
  message: "Select a script to symlink:",
  choices: files.map((f) => ({ title: f, value: f })),
});

if (!file) {
  console.log("No file selected.");
  process.exit(0);
}

const srcPath = await realpath(join(srcDir, file));
const binName = basename(file, ".ts");
const binPath = await realpath(binDir).catch(() => resolve(binDir, "")) + "/" + binName;

// Remove existing symlink if present
try {
  const s = await stat(binPath);
  if (s.isSymbolicLink() || s.isFile()) {
    await unlink(binPath);
  }
} catch {}

// Make sure the source script is executable
await $`chmod +x ${srcPath}`;

// Create symlink
await symlink(srcPath, binPath);
console.log(`Symlinked ${srcPath} -> ${binPath}`);
