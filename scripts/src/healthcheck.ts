#!/usr/bin/env bun

import { $ } from "bun";
import { exitedSuccessfully, info, requireProjectDir } from "./shared";

requireProjectDir();

// process.env.PATH=`${process.env.PATH}:${process.env.HOME}/.local/arenacraft/bin`;

info('Running project setup healthcheck 🤒')
info('------------------------------------')

const cliTools = [
    'cmake',
    'git',
    'cmake',
    'clang',
    'worldserver',
    'authserver',
    'map_extractor',
    'vmap4_extractor',
    'vmap4_assembler',
    'mmaps_generator',
    'mysql',
    'nc'
]

await Promise.all(cliTools.map(checkCliTool));

const services = [
    'arenacraft-world',
    'arenacraft-auth'
];

for (const service of services) {
    const enabled = await exitedSuccessfully($`systemctl --user status ${service}`)
    info(`systemd:         ${(service + '.service').padEnd(32, ' ')} => ${enabled ? '✅' : '❌'}`)
}

const authCheck = await exitedSuccessfully($`nc -zv localhost 3724`)
info(`Auth Running?    ${'auth'.padEnd(32)} => ${authCheck ? '✅' : '❌'}`)
const worldCheck = await exitedSuccessfully($`nc -zv localhost 8085`)
info(`World Running?   ${'world'.padEnd(32)} => ${worldCheck ? '✅' : '❌'}`)

const mysqlCheck = await exitedSuccessfully($`mysql -h 127.0.1 -u acore -pacore --execute "SELECT 1"`)
info(`MySQL check:     ${'mysql'.padEnd(32)} => ${mysqlCheck ? '✅' : '❌'}`)

const redisCheck = await exitedSuccessfully($`nc -zv localhost 6379`)
info(`Redis check:     ${'redis'.padEnd(32)} => ${redisCheck ? '✅' : '❌'}`)


async function checkCliTool(cliTool: string) {
    try {
        await $`which ${cliTool}`.quiet()
        info(`Path check:      ${cliTool.padEnd(32)} => ✅`)
    } catch {
        info(`Path check:      ${cliTool.padEnd(32)} => ❌`)
    }
}