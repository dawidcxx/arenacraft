export function isAbortError(error: unknown, signal?: AbortSignal): boolean {
  if (signal?.aborted) {
    return true;
  }
  return error instanceof DOMException && error.name === "AbortError";
}