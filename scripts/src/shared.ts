import { $ } from "bun"

export async function requireProjectDir() {
    await Promise.all([$`ls src`.quiet(), $`ls apps`.quiet(), $`cat flake.nix`.quiet()]).catch(e => {
        console.error(`Script: '${process.argv[1]}' must be run from the project root directory`)
        process.exit(1)
    })
}

export async function requireProgram(program: string, hint?: string) {
    try {
        await $`which ${program}`.quiet();
    } catch (e) {
        console.error(`Script: '${process.argv[1]}' requires '${program}' to be installed`);
        if (hint) {
            info(`Hint: ${hint}`);
        }
        process.exit(1);
    }
}

// for example 5412589  => 1h30min12s589ms
export function formatElapsedTime(ms: number) {
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

const script = process.argv[1].split('/').pop();
export function info(...messageParams: any[]) {
    console.log(`\x1b[34mINFO ${script}\x1b[0m:`, ...messageParams);
}


export async function exitedSuccessfully(shellCommand: Bun.ShellPromise) {
    try {
        await shellCommand.quiet()
        return true;
    } catch(e) {
        return false
    }
}
