import { describe, expect, it } from "bun:test";
import { getVerifier, isValidSrp6SecurityBuffer } from "./createUser";

describe("getVerifier", () => {
  it("should match snapshot", () => {
    const res = getVerifier(
      "azurerooster",
      "8db753b17ae7",
      Buffer.from(
        "1B0F307C10B0DD20FF705D01DE47741218DDA457BCDCC6E2D0FA31CEFC4DA4DE",
      ),
    );
    expect(res).toMatchSnapshot();
  });
});

describe('isBufferZeroTerminated', () => {
  it('should properly report the result', () => {
    expect(isValidSrp6SecurityBuffer(Buffer.from([1, 2, 2, 3]))).toBe(false) // too short
    expect(isValidSrp6SecurityBuffer(Buffer.from([1, 2, 2, 3, 0]))).toBe(false) // terminated in zero & too short

    const bigBufTerminatedInZero = Buffer.from("99C530A14CFCD09F4FF3768F91C4D811BCE8A53E3DC0A637D55675ED41BAC500", "hex"); // termianted in zero
    expect(isValidSrp6SecurityBuffer(bigBufTerminatedInZero)).toBe(false)

    const bigBufValid = Buffer.from("99C530A14CFCD09F4FF3768F91C4D811BCE8A53E3DC0A637D55675ED41BAC501", "hex");
    expect(isValidSrp6SecurityBuffer(bigBufValid)).toBe(true)
  })
})