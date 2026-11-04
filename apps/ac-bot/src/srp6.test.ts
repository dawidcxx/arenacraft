import { describe, expect, test } from "bun:test";

import { computeVerifier, makeRegistrationData, verifyPassword } from "./srp6";

describe("password verifier", () => {
  test("accepts the password used to create the verifier", () => {
    const { salt, verifier } = makeRegistrationData("Venruki", "hunter2");

    expect(computeVerifier("venruki", "HUNTER2", salt)).toEqual(verifier);
    expect(verifyPassword("venruki", "HUNTER2", salt, verifier)).toBe(true);
  });

  test("rejects the wrong password", () => {
    const { salt, verifier } = makeRegistrationData("Venruki", "hunter2");

    expect(verifyPassword("Venruki", "hunter3", salt, verifier)).toBe(false);
  });
});
