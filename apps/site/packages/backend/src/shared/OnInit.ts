export const OnInit = Symbol("OnInit");

export type OnInit = {
  init(): Promise<void>;
};