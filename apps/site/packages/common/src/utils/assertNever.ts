export function assertNever(x: never): never {
  throw new Error(`AsserNever reached: '${x}'`, { cause: x });
}