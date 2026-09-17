/**
 * Shared database helpers for the scripts tooling.
 *
 * Uses bun's builtin mysql client (`Bun.sql`, adapter "mysql") - no driver
 * dependency needed. Connection strings follow the same format the C++ core
 * expects in AC_*_DATABASE_INFO env variables: "host;port;user;password;db".
 */
import { SQL } from "bun";
import { createHash } from "node:crypto";

export interface DbInfo {
  hostname: string;
  port: number;
  username: string;
  password: string;
  /** only for the target databases, not for the root connection */
  database?: string;
}

/**
 * Parses "hostname;port;username;password;database" as used by the core.
 * `fallback` supplies defaults for missing/empty parts, so env variables
 * can also be partial, e.g. "localhost;;;;acore_auth".
 */
export function parseDbInfo(value: string | undefined, fallback: DbInfo): DbInfo {
  if (!value) return fallback;
  const parts = value.split(";").map((p) => p.trim());
  const [hostname, port, username, password, database] = parts;
  return {
    hostname: hostname || fallback.hostname,
    port: Number(port) || fallback.port,
    username: username || fallback.username,
    password: password ?? fallback.password,
    database: database || fallback.database,
  };
}

export function connect(info: DbInfo): SQL {
  return new SQL({
    adapter: "mysql",
    hostname: info.hostname,
    port: info.port,
    username: info.username,
    password: info.password,
    ...(info.database ? { database: info.database } : {}),
  });
}

/**
 * Splits a .sql file into individual statements. Strips `--` and `#` line
 * comments as well as plain block comments, while keeping mysql conditional
 * comments (the `/*!` variant, e.g. used for LOCK/UNLOCK KEYS directives)
 * and everything inside single/double/backtick quotes intact.
 */
export function splitStatements(sqlText: string): string[] {
  const statements: string[] = [];
  let current = "";
  let i = 0;
  const n = sqlText.length;

  const pushStatement = () => {
    const trimmed = current.trim();
    if (trimmed) statements.push(trimmed);
    current = "";
  };

  while (i < n) {
    const c = sqlText[i];
    const next = sqlText[i + 1];

    // line comments
    if ((c === "-" && next === "-") || c === "#") {
      const eol = sqlText.indexOf("\n", i);
      i = eol === -1 ? n : eol;
      current += " ";
      continue;
    }

    // mysql conditional comments are directives and must survive
    if (c === "/" && next === "*" && sqlText[i + 2] === "!") {
      const end = sqlText.indexOf("*/", i + 3);
      const stop = end === -1 ? n : end + 2;
      current += sqlText.slice(i, stop);
      i = stop;
      continue;
    }

    // plain block comments
    if (c === "/" && next === "*") {
      const end = sqlText.indexOf("*/", i + 2);
      i = end === -1 ? n : end + 2;
      current += " ";
      continue;
    }

    // quoted regions are copied verbatim
    if (c === "'" || c === '"' || c === "`") {
      const end = findQuoteEnd(sqlText, i, c);
      current += sqlText.slice(i, end);
      i = end;
      continue;
    }

    if (c === ";") {
      pushStatement();
      i++;
      continue;
    }

    current += c;
    i++;
  }
  pushStatement();
  return statements;
}

function findQuoteEnd(text: string, start: number, quote: string): number {
  let i = start + 1;
  while (i < text.length) {
    if (text[i] === "\\") {
      i += 2; // escaped char
      continue;
    }
    if (text[i] === quote) {
      if (text[i + 1] === quote) {
        i += 2; // doubled quote, stays inside
        continue;
      }
      return i + 1;
    }
    i++;
  }
  return text.length;
}

/** sha1 of a file, matching the convention recorded in the `updates` table. */
export async function sha1File(path: string): Promise<string> {
  const bytes = await Bun.file(path).bytes();
  return createHash("sha1").update(bytes).digest("hex");
}

/** backtick-escapes an identifier for direct interpolation into DDL. */
export function quoteIdent(name: string): string {
  return "`" + name.replaceAll("`", "``") + "`";
}

/** single-quote-escapes a string literal for direct interpolation into DDL. */
export function quoteStr(value: string): string {
  return "'" + value.replaceAll("'", "''") + "'";
}
