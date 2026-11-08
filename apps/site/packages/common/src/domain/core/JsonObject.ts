import { boolean, GenericSchema, lazy, number, string, union, null_, array, record } from "valibot";

// Represents a blob of opaque, json serializable obj

export type JsonObject =
  | string
  | number
  | boolean
  | null
  | JsonObject[]
  | { [key: string]: JsonObject };

export const JsonObject: GenericSchema<JsonObject> = lazy(() =>
  union([string(), number(), boolean(), null_(), array(JsonObject), record(string(), JsonObject)]),
);