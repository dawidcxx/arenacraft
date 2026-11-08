import { type Context } from "hono";

export function getClientIP(c: Context): string | null {
  const req = c.req;
  for (const header of IP_HEADERS) {
    const value = req.header(header);
    if (value) {
      // x-forwarded-for can contain multiple IPs (comma separated)
      if (header === "x-forwarded-for") {
        return value.split(",")[0]!.trim();
      }
      return value;
    }
  }

  // Last resort: check raw connection (platform-dependent)
  // Note: This may not work in serverless environments
  const raw = c.req.raw as any;
  if (raw.socket?.remoteAddress) {
    return raw.socket.remoteAddress;
  }

  if (raw.connection?.remoteAddress) {
    return raw.connection.remoteAddress;
  }

  return null;
}

// Check common headers in order of reliability
const IP_HEADERS = [
  "cf-connecting-ip", // Cloudflare
  "x-client-ip", // Common
  "x-forwarded-for", // Most common with proxies
  "x-real-ip", // Nginx
  "x-cluster-client-ip", // Rackspace LB, Riverbed Stingray
  "forwarded-for", // RFC 7239
  "forwarded", // RFC 7239
  "x-forwarded", // General
  "x-appengine-user-ip", // Google App Engine
];