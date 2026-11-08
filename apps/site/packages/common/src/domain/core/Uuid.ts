import { parse as vParse, pipe, safeParse, string, transform, uuid } from "valibot";

export type Uuid = string & { __type: "UUID" };

export const Uuid = pipe(
  string(),
  uuid(),
  transform((it) => it as Uuid),
);

export function generateId() {
  return crypto.randomUUID() as Uuid;
}

export function displayShortUuid(uuid: Uuid): string {
  return `#${uuid.slice(0, 6)}`;
}

export function isUuid(inputString: string): inputString is Uuid {
  return safeParse(Uuid, inputString).success;
}

export function uuidFromString(inputString: string): Uuid {
  return vParse(Uuid, inputString);
}

export function uuidFromStringOrNull(inputString: string): Uuid | null {
  const uuid = safeParse(Uuid, inputString);
  if (uuid.success) {
    return uuid.output;
  } else {
    return null;
  }
}

export function uuidFromStringUnchecked(inputString: string): Uuid {
  return inputString as Uuid;
}