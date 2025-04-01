#! /usr/bin/env bun

import { $, ShellError } from 'bun'
import { requireProjectDir } from "./shared";

requireProjectDir();


const timestamp = new Date().toISOString().replace(/[:.]/g, "-");
const backupFile = `backups/mysql-snapshot-${timestamp}.tar.gz`;
await $`mkdir -p backups`;

try {
    await $`docker run --rm -v mysql-data:/data -v $(pwd)/backups:/backup alpine tar -czf /backup/${backupFile} -C /data .`
} catch (e) {
    const er = e as ShellError;
    console.error("Error creating MySQL snapshot:", er.stderr.toString('utf-8'));
    process.exit(1);
}


console.log(`MySQL snapshot created: ${backupFile}`);