import type { HttpStatusCodes } from "@arenacraft/common";

type HttpErrorMetadata = {
  status: (typeof HttpStatusCodes)[keyof typeof HttpStatusCodes];
};

const httpErrorMetadata = new WeakMap<Function, HttpErrorMetadata>();

export function HttpError(meta: HttpErrorMetadata) {
  return function <T extends new (...args: any[]) => Error>(value: T) {
    httpErrorMetadata.set(value, meta);
  };
}

export function getHttpErrorMetadata(error: Error) {
  return httpErrorMetadata.get(error.constructor as Function);
}