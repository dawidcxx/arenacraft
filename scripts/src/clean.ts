#!/usr/bin/env bun

import { $ } from 'bun'
import { requireProjectDir } from './shared'

requireProjectDir()

await $`
    rm -rf ./build
`