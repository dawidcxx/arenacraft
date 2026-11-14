import { array, InferOutput } from "valibot";
import { Streamer } from "../domain/streamers";

// Ordered for display: featured live channels, other live channels, then offline channels.
export const StreamersResponse = array(Streamer);
export type StreamersResponse = InferOutput<typeof StreamersResponse>;

export const streamersContract = {
  list: {
    method: "GET",
    path: "/streamers",
    response: StreamersResponse,
  },
} as const;
