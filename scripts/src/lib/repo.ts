/** Repo layout helpers shared by the scripts tools. */
import { resolve } from "node:path";

/** absolute path to the repository root (this file lives in scripts/src/lib) */
export const repoRoot = resolve(import.meta.dir, "..", "..", "..");
