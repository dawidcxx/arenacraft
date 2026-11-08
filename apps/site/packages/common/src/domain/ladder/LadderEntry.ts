import { InferOutput, minLength, number, object, picklist, pipe, string } from "valibot";

// SoloQ role. Hybrid classes may not have a stored role yet, hence the empty option.
export const LadderRole = picklist(["Melee", "Caster", "Healer", ""]);
export type LadderRole = InferOutput<typeof LadderRole>;

export const LadderEntry = object({
  name: pipe(string(), minLength(1)),
  class: pipe(string(), minLength(1)),
  role: LadderRole,
  rating: number(),
  wins: number(),
  losses: number(),
});

export type LadderEntry = InferOutput<typeof LadderEntry>;