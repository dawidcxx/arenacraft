import { createFetchClient, ladderContract } from "@arenacraft/common";

export const ladderApi = createFetchClient(ladderContract, { baseUrl: "/api" });