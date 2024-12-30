#!/usr/bin/env bun

import { $ } from 'bun'

const config = getConfig()
await requireProgram('rsync')
await requireProjectDir()

const remoteDir = '/opt/patch-files'

const directories = ['DBFilesClient', 'Interface']
const uploadDirectory = (dir) => {
    info(`Uploading '${config.filesDir}/${dir}' to remote '${remoteDir}'`)
    return $`rsync -avz ${config.filesDir}/${dir} ${config.sshUrl}:${remoteDir} 2>&1`.quiet()
}
await Promise.all(directories.map(uploadDirectory))

info('Assets uplaoding, making release')

await $`curl -sS -X POST ${config.releaseManagerUrl}/releases`.quiet()

info('Deploy assets script complete')

//  
// UTILS
//
function getConfig() {
    process.title = import.meta.file
    return { sshUrl: requireEnv('SSH_URL'), filesDir: requireEnv('FILES_DIR'), releaseManagerUrl: requireEnv('RELEASE_MANAGER_URL') }
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