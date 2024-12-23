export function isNil(value: any): value is null | undefined {
  return value === null || value === undefined;
}

export function requireNotNull<T>(
  possiblyNull: T | null | undefined,
): asserts possiblyNull is T {
  if (isNil(possiblyNull)) {
    throw new Error("notNull assertion failed");
  }
}

export function randomInt(from: number = 1, to: number = 100) {
  const minCeiled = Math.ceil(from);
  const maxFloored = Math.floor(to);
  return Math.floor(Math.random() * (maxFloored - minCeiled) + minCeiled);
}
