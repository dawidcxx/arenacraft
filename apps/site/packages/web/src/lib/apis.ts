import {
  createFetchClient,
  ladderContract,
  playerOverlayContract,
  streamersContract,
} from "@arenacraft/common";

export const ladderApi = createFetchClient(ladderContract, {
  baseUrl: `${import.meta.env.BASE_URL}api`,
});

export const playerOverlayApi = createFetchClient(playerOverlayContract, {
  baseUrl: `${import.meta.env.BASE_URL}api`,
});

export const streamersApi = createFetchClient(streamersContract, {
  baseUrl: `${import.meta.env.BASE_URL}api`,
});
