#!/usr/bin/env bun

import { $, ShellError } from "bun";
import {
  formatElapsedTime,
  info,
  requireProgram,
  requireProjectDir,
} from "./shared";
import { realpathSync } from "node:fs";

const DEST = realpathSync(`${process.env.HOME}/.local/arenacraft`);

const withClangDrefresh = process.argv.includes("--refresh-clangd");

requireProjectDir();
requireProgram("cmake", "Did you forget to run nix develop?");

await $`mkdir ~/.local`.quiet().nothrow();
await $`mkdir ~/.local/arenacraft`.quiet().nothrow();
await $`mkdir ~/.local/var`.quiet().nothrow();

info(`Installing to: '${DEST}'`);

await $`
    mkdir build
    mkdir ~/.local
    mkdir ~/.local/arenacraft
    mkdir ~/.local/var
`
  .quiet()
  .nothrow();

const buildCommand = [
  "cmake ../",
  '-G "Ninja"',
  "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
  "-DTOOLS_BUILD=all",
  ...(withClangDrefresh ? ["-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"] : []),
  `-DCMAKE_INSTALL_PREFIX="${DEST}"`,
].join(" ");

try {
  const started = Date.now();
  info("Building ArenaCraft");
  if (withClangDrefresh) {
    info("Clangd compile commands will be regenerated");
  }
  info("Build flags: " + buildCommand);
  await $`
        cd build
        ${{ raw: buildCommand }}
        cmake --build . --target install
    `.quiet();
  const elapsed = Date.now() - started;
  info(`Building ArenaCraft done in ${formatElapsedTime(elapsed)}`);
  if (withClangDrefresh) {
    await $`
      cp ./build/compile_commands.json .
    `.nothrow();
  }
} catch (e) {
  const er = e as ShellError;
  console.error("Build failed");
  console.log(er.stderr.toString("utf-8"));
  process.exit(1);
}

await setupConfigFiles();

// for example authserver.conf.dist => authserver.conf
async function setupConfigFiles() {
  for await (const file of $`ls ${DEST}/etc`.lines()) {
    if (file.endsWith(".dist")) {
      const newFile = file.replace(".dist", "");
      await $`cp -n ${DEST}/etc/${file} ${DEST}/etc/${newFile}`
        .quiet()
        .nothrow();
    }
  }
  for await (const file of $`ls ${DEST}/etc/modules`.lines()) {
    if (file.endsWith(".dist")) {
      const newFile = file.replace(".dist", "");
      await $`cp -n ${DEST}/etc/modules/${file} ${DEST}/etc/modules/${newFile}`
        .quiet()
        .nothrow();
    }
  }
}

const systemdWorldService = [
  "arenacraft-world",
  `
[Unit]
Description=Arenacraft World Service
After=network.target

[Service]
WorkingDirectory=%h/.local/arenacraft
ExecStart=%h/.local/arenacraft/bin/worldserver

[Install]
WantedBy=default.target
`,
];

const systemdAuthService = [
  "arenacraft-auth",
  `
[Unit]
Description=Arenacraft Auth Service
After=network.target
[Service]
WorkingDirectory=%h/.local/arenacraft
ExecStart=%h/.local/arenacraft/bin/authserver
[Install]
WantedBy=default.target
`,
];

for (const [name, content] of [systemdWorldService, systemdAuthService]) {
  await $`mkdir -p ~/.config/systemd/user`.quiet().nothrow();
  await $`touch ~/.config/systemd/user/${name}.service`.quiet().nothrow();
  await Bun.file(
    `${process.env.HOME}/.config/systemd/user/${name}.service`
  ).write(content);
}
