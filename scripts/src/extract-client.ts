#!/usr/bin/env bun

import { $ } from "bun";
import prompts from "prompts";
import { formatElapsedTime, info, requireProgram, requireProjectDir } from "./shared";

const { GAME_CWD } = await prompts([{
    type: 'text',
    name: 'GAME_CWD',
    message: 'Path to your game directory',
    initial: `${process.env.HOME}/.local/var/wow`
}]) as { GAME_CWD: string }
info(`Using game directory: ${GAME_CWD}`)

const { DEST } = await prompts([{
    type: 'text',
    name: 'DEST',
    message: 'Path to your destination directory',
    initial: `${process.env.HOME}/.local/arenacraft/data`
}]) as { DEST: string }

info(`Using destination directory: ${DEST}`)

await requireProjectDir();
await requireInstalledClient();

await requireProgram("map_extractor");
await requireProgram("vmap4_extractor");
await requireProgram("vmap4_assembler");
await requireProgram("mmaps_generator");

info(
    `Upon completaion your files will be stored: '${DEST}'`,
);

const response = await prompts([
    {
        type: "text",
        name: "mmaps",
        message:
            "Would you also like to run mmaps_generator (takes most of the time) [y/N]",
        initial: "N",
    },
]);

const config = {
    extractMmaps: response.mmaps === "y" || response.mmaps === "Y",
};

info("CONFIGURATION", config);

const { confirm } = await prompts({
    type: "confirm",
    name: "confirm",
    message: "Start extracting client data?",
    initial: true,
});

if (!confirm) {
    info("Exiting..");
    process.exit(0);
}

const start = performance.now()

await $`mkdir -p ${DEST}`.quiet();
await $`rm -rf Buildings cameras dbc db2 maps mmaps vmaps`
    .cwd(GAME_CWD)
    .quiet()
    .nothrow();
info('Removed old data & setup complete')

info("Extracting maps & vmaps.. This will take a while..");
await Promise.all([
    $`map_extractor`
        .cwd(GAME_CWD)
        .quiet()
        .then(() => info("Map extraction complete")),
    $`vmap4_extractor`
        .cwd(GAME_CWD)
        .quiet()
        .then(() => info("Vmap extraction complete")),
]);


info("Assembling vmaps..");
await $.cwd(GAME_CWD)`
    mkdir vmaps
    vmap4_assembler Buildings vmaps
`.quiet()
info("VMap assembly complete");

if (config.extractMmaps) {
    info("Generating mmaps.. This will take a while..");
    await $.cwd(GAME_CWD)`
        mkdir mmaps
        mmaps_generator
    `.quiet();
    info("Mmaps generation complete");
}

info("Copying data to server directory..");
await $`cp -r dbc maps vmaps ${config.extractMmaps ? 'mmaps' : ''} Cameras ${DEST}`
    .cwd(GAME_CWD)
    .quiet();


const elapsedInMs = Math.round(performance.now() - start)
info(`Finished in ${formatElapsedTime(elapsedInMs)}`)


async function requireInstalledClient() {
    await $`ls ${GAME_CWD}`.quiet().catch(() => {
        console.error(
            `Game files need to be present in '${GAME_CWD}'. Please drop your client to '${GAME_CWD}'`,
        );
        process.exit(1);
    });
    await $`ls ${GAME_CWD}/Data`.quiet().catch(() => {
        throw new Error(`Missing Data directory in: '${GAME_CWD}'`);
    });
}
