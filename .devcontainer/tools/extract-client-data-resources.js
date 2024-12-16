#!/usr/bin/env bun

import { $ } from "bun";
import prompts from "prompts";

await requireProjectDir();
await requireInstalledClient(["/opt/game/Wow.exe", "/opt/game/Wow-64.exe"]);

await requireProgram("map_extractor");
await requireProgram("vmap4_extractor");
await requireProgram("vmap4_assembler");
await requireProgram("mmaps_generator");

const GAME_CWD = "/opt/game";

info(
    "Upon completaion your files will be stored {SOURCE_ROOT}/.devcontainer/server/data",
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

await $`mkdir -p /usr/local/server/data`.quiet();
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
await $`cp -r dbc maps vmaps mmaps Cameras /usr/local/server/data`
    .cwd(GAME_CWD)
    .quiet();


const elapsedInMs = Math.round(performance.now() - start)
info(`Finished in ${formatElapsedTime(elapsedInMs)}`)

//
// UTILS
//
async function requireProjectDir() {
    await $`ls .devcontainer`.quiet().catch((e) => {
        throw new Error("Not in project directory");
    });
}

async function requireProgram(program) {
    try {
        await $`which ${program}`.quiet();
    } catch (e) {
        throw new Error(`Program '${program}' is not installed`);
    }
}


function info(...messageParams) {
    console.log(`\x1b[34mINFO ${import.meta.file}\x1b[0m:`, ...messageParams);
}

// for example 5412589  => 1h30min12s589ms
function formatElapsedTime(ms) {
    const milliseconds = ms % 1000;
    const seconds = Math.floor((ms / 1000) % 60);
    const minutes = Math.floor((ms / (1000 * 60)) % 60);
    const hours = Math.floor((ms / (1000 * 60 * 60)) % 24);
    const parts = [];
    if (hours > 0) parts.push(`${hours}h`);
    if (minutes > 0) parts.push(`${minutes}min`);
    if (seconds > 0) parts.push(`${seconds}s`);
    if (milliseconds > 0 || parts.length === 0) parts.push(`${milliseconds}ms`);
    return parts.join('').trim();
}

/**
 *
 * @param {Array<string>} candidates
 */
async function requireInstalledClient(candidates) {
    await $`ls /opt/game`.quiet().catch((e) => {
        throw new Error("Files need to be present in /opt/game", { cause: e });
    });
    await $`ls /opt/game/Data`.quiet().catch((e) => {
        throw new Error("Missing Data directory", { cause: e });
    });

    const hasRequiredFiles = (
        await Promise.all(candidates.map((it) => Bun.file(it).exists()))
    ).some((exists) => exists);
    if (!hasRequiredFiles) {
        info(
            "Missing wow files. Please drop your client to {SOURCE_ROOT}/.devcontainer/game",
        );
        throw new Error("Failed to extract client data, exitting..");
    }
}
