import { build } from "bun";

const deps = Object.keys((await Bun.file("./package.json").json()).dependencies);

await build({
  entrypoints: ["./src/index.ts"],
  outdir: "./dist",
  external: deps,
  target: "browser",
  format: "esm",
});