import { createHash, randomBytes } from "node:crypto";

// Port of Acore::Crypto::SRP6::MakeRegistrationData (src/common/Cryptography).
// The auth core uppercases the client-sent login before SRP6 math
// (src/auth/Server/AuthSession.cpp: Utf8ToUpperOnlyLatin), so both the
// username and password are uppercased here before deriving the verifier --
// regardless of how the name is stored in the DB.
//
//   salt     = 32 random bytes
//   x        = SHA1(salt || SHA1(username || ":" || password))   (digest read LE)
//   verifier = g^x mod N                                         (32 bytes LE)
const N = BigInt("0x894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7");
const G = 7n;

function sha1(...buffers: Uint8Array[]): Buffer {
  const hash = createHash("sha1");
  for (const buffer of buffers) hash.update(buffer);
  return hash.digest();
}

function modPow(base: bigint, exponent: bigint, modulus: bigint): bigint {
  let result = 1n;
  let b = base % modulus;
  let e = exponent;
  while (e > 0n) {
    if (e & 1n) result = (result * b) % modulus;
    b = (b * b) % modulus;
    e >>= 1n;
  }
  return result;
}

function bytesToBigIntLE(bytes: Uint8Array): bigint {
  let value = 0n;
  for (let i = bytes.length - 1; i >= 0; i--) value = (value << 8n) | BigInt(bytes[i]!);
  return value;
}

function bigIntToBytesLE(value: bigint, length: number): Buffer {
  const out = Buffer.alloc(length);
  let v = value;
  for (let i = 0; i < length; i++) {
    out[i] = Number(v & 0xffn);
    v >>= 8n;
  }
  return out;
}

export interface RegistrationData {
  salt: Buffer;
  verifier: Buffer;
}

export function makeRegistrationData(username: string, password: string): RegistrationData {
  const upperUser = username.toUpperCase();
  const upperPass = password.toUpperCase();
  const salt = randomBytes(32);
  const inner = sha1(
    Buffer.from(upperUser, "utf8"),
    Buffer.from(":", "utf8"),
    Buffer.from(upperPass, "utf8"),
  );
  const x = sha1(salt, inner);
  const verifier = modPow(G, bytesToBigIntLE(x), N);
  return { salt, verifier: bigIntToBytesLE(verifier, 32) };
}
