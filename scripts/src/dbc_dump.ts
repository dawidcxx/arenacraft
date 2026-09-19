#!/usr/bin/env bun
/**
 * dbc_dump - inspect a WoW 3.3.5 client database (WDBC) file.
 *
 * Examples:
 *   bun scripts/src/dbc_dump.ts Faction --name-field 23 --search cenarion
 *   bun scripts/src/dbc_dump.ts data/dbc/Faction.dbc -n 23 -f 1,18
 *   bun scripts/src/dbc_dump.ts Spell -n 136 --json --limit 5
 */
import { Command } from "commander";
import { readDbc, resolveDbcPath } from "./lib/dbc.ts";

interface Options {
  nameField?: string;
  idField: string;
  fields?: string;
  search?: string;
  limit?: string;
  json?: boolean;
}

const program = new Command()
  .name("dbc_dump")
  .argument("<dbc>", "path to a .dbc file, or a name under data/dbc (e.g. Faction)")
  .option("-n, --name-field <n>", "field index to decode as a string (e.g. 23 for Faction names)")
  .option("-i, --id-field <n>", "field index to treat as the row id", "0")
  .option("-f, --fields <list>", "comma-separated extra field indices to print")
  .option("-s, --search <text>", "case-insensitive filter on the decoded name field")
  .option("-l, --limit <n>", "stop after n matching rows")
  .option("--json", "emit JSON instead of TSV")
  .parse();

const opts = program.opts<Options>();
const file = readDbc(resolveDbcPath(program.args[0]!));

const idField = Number(opts.idField);
const nameField = opts.nameField === undefined ? undefined : Number(opts.nameField);
const extra = opts.fields ? opts.fields.split(",").map(Number) : [];
const search = opts.search?.toLowerCase();
const limit = opts.limit === undefined ? Infinity : Number(opts.limit);

const nameOf = (rec: Uint32Array) => (nameField === undefined ? undefined : file.string(rec[nameField] ?? 0));

const rows = file.records
  .map((rec) => ({ id: rec[idField] ?? 0, name: nameOf(rec), values: extra.map((f) => rec[f] ?? 0) }))
  .filter((row) => (search === undefined || (row.name ?? "").toLowerCase().includes(search)))
  .slice(0, limit);

if (opts.json) {
  console.log(JSON.stringify(rows, null, 2));
} else {
  for (const row of rows) {
    const cols: (string | number)[] = [row.id];
    if (row.name !== undefined) cols.push(row.name);
    cols.push(...row.values);
    console.log(cols.join("\t"));
  }
}
