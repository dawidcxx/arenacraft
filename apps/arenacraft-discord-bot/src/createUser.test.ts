import { describe, expect, it, test } from "bun:test";
import { getVerifier, isBufferZeroTerminated } from "./createUser";

describe("createUser tests", () => {
  test("getVerifier", () => {
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

  test('isBufferZeroTerminated', () => {
    it('should properly report the result', () => {
      expect(isBufferZeroTerminated(Buffer.from([1, 2, 2, 3]))).toBe(false)
      expect(isBufferZeroTerminated(Buffer.from([1, 2, 2, 3, 0]))).toBe(true)
    })
  })
});
