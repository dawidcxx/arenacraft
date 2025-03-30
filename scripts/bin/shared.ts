import { $ } from "bun"

export async function requireProjectDir() {
    await Promise.all([$`ls src`.quiet(), $`ls apps`.quiet(), $`ls modules`]).catch(e => {
        console.error(`Script: '${process.argv[1]}' must be run from the project root directory`)
        process.exit(1)
    })
}
