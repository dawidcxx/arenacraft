#!/usr/bin/env bun

import { $ } from "bun";
import { requireProjectDir } from "./shared";
import { symlink, unlink } from "fs/promises";
import { info } from "console";

await requireProjectDir();

const baseDir =
  await $`realpath ${process.env.HOME}/.local/arenacraft/etc`.text().then(it => it.trim());

info(`Base dir is '${baseDir}'`);

const files = ["worldserver.conf", "authserver.conf", "dbimport.conf"];
for (const file of files) {
  await unlink(file).catch((err) => {
    if (err.code !== "ENOENT") throw err;
  });
  await symlink(`${baseDir}/${file}`, file).catch((err) => {
    if (err.code !== "EEXIST") throw err;
  });
}
