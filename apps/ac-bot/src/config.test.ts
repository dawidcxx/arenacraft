import { describe, expect, test } from "bun:test";

import { authDbUrlFromEnv, getConfig } from "./config";

const BASE_ENV = {
  DISCORD_TOKEN: "token",
  DISCORD_GUILD_ID: "1",
  CHARACTER_DB_URL: "mysql://acore:acore@127.0.0.1:3306/acore_characters",
};

describe("authDbUrlFromEnv", () => {
  test("prefers AUTH_DB_URL", () => {
    expect(authDbUrlFromEnv({ ...BASE_ENV, AUTH_DB_URL: "mysql://a:b@db:3306/auth" })).toBe(
      "mysql://a:b@db:3306/auth",
    );
  });

  test("falls back to AC_LOGIN_DATABASE_INFO", () => {
    expect(authDbUrlFromEnv({ ...BASE_ENV, AC_LOGIN_DATABASE_INFO: "db;3306;acore;acore;acore_auth" })).toBe(
      "mysql://acore:acore@db:3306/acore_auth",
    );
  });

  test("returns null when neither is usable", () => {
    expect(authDbUrlFromEnv({})).toBeNull();
    expect(authDbUrlFromEnv({ AC_LOGIN_DATABASE_INFO: "db;3306;acore" })).toBeNull();
  });
});

describe("getConfig", () => {
  test("accepts AUTH_DB_URL directly", () => {
    const config = getConfig({ ...BASE_ENV, AUTH_DB_URL: "mysql://a:b@db:3306/auth" });
    expect(config.AUTH_DB_URL).toBe("mysql://a:b@db:3306/auth");
  });

  test("derives AUTH_DB_URL from the core database env", () => {
    const config = getConfig({ ...BASE_ENV, AC_LOGIN_DATABASE_INFO: "db;3306;acore;acore;acore_auth" });
    expect(config.AUTH_DB_URL).toBe("mysql://acore:acore@db:3306/acore_auth");
  });
});
