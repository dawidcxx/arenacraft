import type { Connection } from "mysql2/promise";
import { createHash, randomBytes } from 'crypto'
import { toBigIntLE } from 'bigint-buffer'
import BigInteger from 'big-integer'
import { requireNotNull } from "./utils";
import { AlreadyRegistered } from "./error";



/**
 * 
 * @param connection 
 * @param params 
 */
export async function createUser(connection: Connection, params: {
    username: string;
    password: string;
    discordId: string;
}) {
    const [rows] = await connection.execute(`SELECT 1 from account where email = ?`, [params.discordId])
    if (Array.isArray(rows) && rows.length > 0) {
        throw new AlreadyRegistered();
    }
    const salt = randomBytes(32);
    const verifier = getVerifier(params.username, params.password, salt);
    const registration = {
        username: params.username,
        salt,
        verifier,
        email: params.discordId
    }
    await connection.execute(`INSERT into account (username, salt, verifier, email) values
                                    (:username, :salt, :verifier, :email)`, registration);
}


// taken from: https://github.com/trinity-flux/srp6/blob/main/src/utils/index.js
//
// To obtain the verifier:
// Calculate h1 = SHA1("USERNAME:PASSWORD"), substituting the user's username and password converted to uppercase
// This is the old sha_pass_hash value!
// Calculate h2 = SHA1(salt || h1), where || is concatenation (the . operator in PHP)
// Note that both salt and h1 are binary, not hexadecimal strings!
// Treat h2 as an integer in little-endian order (the first byte is the least significant)
// Calculate (g ^ h2) % N
// ^ is modular exponentiation, % is the modulo operator
// g and N are parameters, which are fixed in the WoW implementation:
// g = 7
// N = 0x894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7
// Convert the result back to a byte array in little-endian order
// This is your verifier - you're done!
// Content
export function getVerifier(
    username: string,
    password: string,
    salt: Buffer
): Buffer {
    const firstHash = createHash('sha1')
        .update(`${username.toUpperCase()}:${password.toUpperCase()}`)
        .digest();
    const secondHash = createHash('sha1')
        .update(salt)
        .update(firstHash)
        .digest();
    const littleEndianInteger = toBigIntLE(secondHash);
    const littleEndianIntegerPow = BigInteger(g).modPow(littleEndianInteger, N);
    const littleEndianIntegerOrder = littleEndianIntegerPow.toString(16).match(/.{2}/g)?.reverse().join('');
    requireNotNull(littleEndianIntegerOrder);
    const verifier = Buffer.from(littleEndianIntegerOrder, 'hex');
    return verifier;
}

const g = BigInt(7);
const N = BigInt(
    "0x894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7"
);