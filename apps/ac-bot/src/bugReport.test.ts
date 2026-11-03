import { describe, expect, test } from "bun:test";

import { buildBugModal, formatBugReport, guessTags } from "./bugReport";
import { BUG_TAGS } from "./serverLayout";

describe("guessTags", () => {
  test("tags the class named in the form and marks the report open", () => {
    expect(guessTags("Mage - Blink", BUG_TAGS)).toEqual(["Mage", "Open"]);
    expect(guessTags("dk grip", BUG_TAGS)).toEqual(["Death Knight", "Open"]);
    expect(guessTags("Frost DK vs Resto Druid", BUG_TAGS)).toEqual(["Death Knight", "Druid", "Open"]);
  });

  test("falls back to General and does not match inside other words", () => {
    expect(guessTags("", BUG_TAGS)).toEqual(["General", "Open"]);
    expect(guessTags("damage numbers", BUG_TAGS)).toEqual(["General", "Open"]); // "damage" contains "mage"
  });

  test("only uses tags the forum has", () => {
    expect(guessTags("Mage", ["Open"])).toEqual(["Open"]);
  });
});

describe("formatBugReport", () => {
  test("lists the filled sections and mentions the reporter", () => {
    const text = formatBugReport({
      title: "t",
      classText: "Mage",
      what: "It broke",
      steps: "",
      expected: "It works",
      reporterId: "42",
      reporterName: "someone",
    });
    expect(text).toContain("<@42>");
    expect(text).toContain("**What happened**\nIt broke");
    expect(text).toContain("**Expected**\nIt works");
    expect(text).not.toContain("How to reproduce");
  });
});

test("the bug form fits Discord's limit of five inputs", () => {
  expect(buildBugModal().toJSON().components).toHaveLength(5);
});
