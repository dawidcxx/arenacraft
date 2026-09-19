/** Minimal reader for WoW 3.3.5 client database (WDBC) files. */
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { repoRoot } from "./repo.ts";

export interface DbcFile {
  fieldCount: number;
  recordSize: number;
  /** raw uint32 fields per record */
  records: Uint32Array[];
  /** NUL-terminated string block */
  stringBlock: Buffer;
  /** decode a value from a record's string field */
  string(fieldValue: number): string;
}

/** Parse a .dbc file from disk. */
export function readDbc(path: string): DbcFile {
  const buf = readFileSync(path);

  if (buf.length < 20 || buf.toString("ascii", 0, 4) !== "WDBC")
    throw new Error(`${path}: not a WDBC file`);

  const recordCount = buf.readUInt32LE(4);
  const fieldCount = buf.readUInt32LE(8);
  const recordSize = buf.readUInt32LE(12);
  const stringBlockSize = buf.readUInt32LE(16);

  const recordsOffset = 20;
  const stringOffset = recordsOffset + recordCount * recordSize;
  const stringBlock = buf.subarray(stringOffset, stringOffset + stringBlockSize);

  const records: Uint32Array[] = [];
  for (let i = 0; i < recordCount; i++) {
    const rec = new Uint32Array(fieldCount);
    for (let f = 0; f < fieldCount; f++)
      rec[f] = buf.readUInt32LE(recordsOffset + i * recordSize + f * 4);
    records.push(rec);
  }

  const string = (fieldValue: number): string => {
    if (fieldValue <= 0 || fieldValue >= stringBlock.length) return "";
    const end = stringBlock.indexOf(0, fieldValue);
    return stringBlock.toString("utf8", fieldValue, end === -1 ? stringBlock.length : end);
  };

  return { fieldCount, recordSize, records, stringBlock, string };
}

/**
 * Resolve a DBC argument the way the server resolves AC_DATA_DIR: a bare name
 * (e.g. `Faction`) or nothing at all looks under `<repo>/data/dbc`.
 */
export function resolveDbcPath(arg: string): string {
  const candidates = [arg, join(repoRoot, "data", "dbc", arg), join(repoRoot, "data", "dbc", `${arg}.dbc`)];
  for (const c of candidates) if (existsSync(c)) return c;
  throw new Error(`cannot find DBC file '${arg}' (try a path or a name under data/dbc)`);
}
