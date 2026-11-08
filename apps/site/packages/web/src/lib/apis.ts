import { createFetchClient, ladderContract } from "@arenacraft/common";

export const ladderApi = createFetchClient(ladderContract, {
  baseUrl: `${import.meta.env.BASE_URL}api`,
});