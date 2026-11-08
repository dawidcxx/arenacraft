export function assertUnreachable(hint: string): never {
  throw new Error(`Assert Unreachable condition reached: '${hint}'`);
}