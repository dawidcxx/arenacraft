export const OnDeinit = Symbol("OnDeinit");

export type OnDeinit = {
  deinit(): Promise<void> | void;
};