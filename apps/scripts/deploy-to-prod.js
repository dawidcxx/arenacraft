#!/usr/bin/env bun

import { $ } from 'bun'

const config = getConfig()

await requireProgram('rsync')
await requireProgram('cmake')
await requireProgram('make')

await requireProjectDir()

info('Building latest app from source..')

await $`
    rm -rf build
    mkdir build
    cd build
    cmake ../ \
        -DCMAKE_BUILD_TYPE=Release \
        -DTOOLS_BUILD=all \
        -DCMAKE_INSTALL_PREFIX=${config.installDir} \
        -DSCRIPTS=static \
        -DMODULES=static \
    /
    make -j$(nproc)
    make install
`.quiet();

info('..Built app from source')

info('Deploying to prod...')

const directories = ['bin', 'data']
const uploadDirectory = (dir) => {
    info(`Uploading '${config.installDir}/${dir}' to remote '/usr/local/server'`)
    return $`rsync -avz ${config.installDir}/${dir} ${config.sshUrl}:/usr/local/server 2>&1`.quiet()
}
await Promise.all(directories.map(uploadDirectory))

info('..Deployed to prod')


//  
// UTILS
//
function getConfig() {
    process.title = import.meta.file
    return { sshUrl: requireEnv('SSH_URL'), installDir: requireEnv('INSTALL_DIR') }
}

async function requireProjectDir() {
    await $`ls .devcontainer`.quiet().catch(e => {
        throw new Error('Not in project directory')
    })
}

async function requireProgram(program) {
    try {
        await $`which ${program}`.quiet()
    } catch (e) {
        throw new Error(`Program '${program}' is not installed`)
    }
}

function getEnv(key) {
    if (typeof process.env[key] === 'string') {
        return process.env[key]
    } else {
        return null
    }
}

function requireEnv(key) {
    const value = getEnv(key)
    if (value === null) {
        throw new Error(`Missing env variable: '${key}'`)
    }
    return value
}

function info(...messageParams) {
    console.log(`\x1b[34mINFO ${import.meta.file}\x1b[0m:`, ...messageParams)
}