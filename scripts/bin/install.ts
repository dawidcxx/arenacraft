#!/usr/bin/env bun

import { $, ShellError } from 'bun'
import { requireProjectDir } from './shared'

requireProjectDir()

await $`
    mkdir build
    mkdir ~/.local
    mkdir ~/.local/arenacraft
`.quiet().nothrow()

try {
await $`
    cd build
    cmake ../ \
        -G "Ninja" \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DTOOLS_BUILD=all \
        -DCMAKE_INSTALL_PREFIX=~/.local/arenacraft  
    cmake --build . --target install
` } catch (e) {
    const er = e as ShellError;
    console.log('Build failed')
    console.log(er.stderr.toString('utf-8'))
}