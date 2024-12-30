export function isNil(value: any): value is null | undefined {
  return value === null || value === undefined;
}

export function requireNotNull<T>(
  possiblyNull: T | null | undefined,
  message: string,
): asserts possiblyNull is T {
  if (isNil(possiblyNull)) {
    throw new Error(`notNull assertion failed ${message}`);
  }
}

export function randomInt(from: number = 1, to: number = 100) {
  const minCeiled = Math.ceil(from);
  const maxFloored = Math.floor(to);
  return Math.floor(Math.random() * (maxFloored - minCeiled) + minCeiled);
}

export function assert(condition: boolean, message: string): asserts condition {
  if (condition !== true) {
    throw new Error(message);
  }
}

const throttleMap = new Map<string, number>();
export function throttle<T>(throttleActionKey: string, action: () => Promise<void>) {
  const lastAction = throttleMap.get(throttleActionKey);
  if (lastAction === undefined || +new Date() - lastAction > 5000) {
    throttleMap.set(throttleActionKey, +new Date());
    return action();
  }
}