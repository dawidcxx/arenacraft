export function assertNotNull<T>(value: T, hint?: string): asserts value is NonNullable<T> {
  if (value == null) {
    throw new Error(
      hint ? `Expected value to be non-null. ${hint}` : "Expected value to be non-null.",
    );
  }
}