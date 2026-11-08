import { $ } from "bun";

await $`rm -rf ./dist`;
await Bun.build({
  entrypoints: ["./src/main.ts"],
  outdir: "./dist",
  target: "node",
  sourcemap: "external",
});
await $`cp -r ../web/dist ./dist/public`.nothrow();