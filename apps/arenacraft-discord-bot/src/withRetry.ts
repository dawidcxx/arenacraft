import { RetryError, RetryOverflowError } from "./error";

export async function withRetry<T>(
  asyncProducer: () => Promise<T>,
  retryCount: number = 10,
  lastErrorMessage: string | null = null,
) {
  try {
    if (retryCount < 1) {
      throw new RetryOverflowError(
        `withRetry failed too many times { lastErrorMessage = '${lastErrorMessage}' }`,
      );
    }
    return asyncProducer();
  } catch (e) {
    if (e instanceof RetryError) {
      console.log(
        `RetryError encountered, retrying.. (retryCount=${retryCount})`,
      );
      return withRetry(asyncProducer, retryCount - 1, e.message);
    } else {
      throw e;
    }
  }
}
