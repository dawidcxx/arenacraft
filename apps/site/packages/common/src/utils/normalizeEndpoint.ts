export function normalizeEndpoint(input: string): string {
  // Trim trailing slash (but keep root '/')
  if (input.endsWith("/") && input.length > 1) {
    return input.slice(0, -1);
  }
  return input;
}