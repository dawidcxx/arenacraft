import { date, InferOutput, pipe, transform, unknown } from "valibot";

export const JsDate = pipe(
  unknown(),
  transform((input) => {
    if (input instanceof Date) return input;
    if (typeof input === "string" || typeof input === "number") {
      const d = new Date(input);
      if (!isNaN(d.getTime())) return d;
    }
    return input; // pass invalid forward
  }),
  date(),
);
export type JsDate = InferOutput<typeof JsDate>;