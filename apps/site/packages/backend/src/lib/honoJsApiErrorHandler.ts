import { HttpStatusCodes, type HttpStatusCode } from "@arenacraft/common";
import type { Context } from "hono";
import type { HTTPResponseError } from "hono/types";
import { serializeError } from "serialize-error";
import { getHttpErrorMetadata } from "./HttpErrorMetadata";
import type { Logger } from "./Logger";

export const honoJsErrorHandler = (logger: Logger, errorHandlers: ErrorHandlerEntries = []) => {
  return (error: Error | HTTPResponseError, c: Context) => {
    for (const [ErrorType, handler] of errorHandlers) {
      if (error instanceof ErrorType) {
        const result = handler(error);
        return c.json(
          {
            code: result.code ?? error.constructor.name,
            message: result.message ?? error.message,
            params: result.params,
          },
          result.status ?? HttpStatusCodes.INTERNAL_SERVER_ERROR,
        );
      }
    }

    const meta = getHttpErrorMetadata(error);
    if (!meta) {
      logger.error("Unknown error occured in handler", { error: serializeError(error) });
      return c.json(
        {
          code: "UnknownError",
          message: "A unknown error has occured",
        },
        HttpStatusCodes.INTERNAL_SERVER_ERROR,
      );
    }
    return c.json(
      {
        code: error.constructor.name,
        message: error.message,
        params: "params" in error ? error.params : undefined,
      },
      meta.status,
    );
  };
};

type ErrorConstructor<T extends Error = Error> = new (...args: any[]) => T;

type ErrorHandlerResult = {
  code?: string;
  message?: string;
  params?: unknown;
  status?: HttpStatusCode;
};

type ErrorHandlerEntry<T extends Error = Error> = readonly [
  ErrorConstructor<T>,
  (error: T) => ErrorHandlerResult,
];

type ErrorHandlerEntries = readonly ErrorHandlerEntry[];