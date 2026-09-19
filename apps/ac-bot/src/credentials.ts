import { randomInt } from "node:crypto";

// Ambiguous glyphs (I, O, 0, 1) are left out so passwords survive copy/paste by
// hand. Auth is case-insensitive (the verifier uppercases both sides), so we
// only ever generate uppercase.
const ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
const DEFAULT_LENGTH = 16;

export function generatePassword(length: number = DEFAULT_LENGTH): string {
  let password = "";
  for (let i = 0; i < length; i++) password += ALPHABET[randomInt(ALPHABET.length)]!;
  return password;
}
