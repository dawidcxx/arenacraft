import { array, InferOutput } from "valibot";
import { LadderEntry } from "../domain/ladder";

export const LadderResponse = array(LadderEntry);
export type LadderResponse = InferOutput<typeof LadderResponse>;

export const ladderContract = {
  soloq: {
    method: "GET",
    path: "/ladder/soloq",
    response: LadderResponse,
  },
} as const;