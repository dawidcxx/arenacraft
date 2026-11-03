import { expect, test } from "bun:test";

import { supportPanel, welcomePanel, WELCOME_PANEL } from "./onboarding";
import { CLASSES, LANGUAGES } from "./serverLayout";

test("the welcome panel has one button per language and a class menu with every class", () => {
  const panel = welcomePanel("<#1>");
  const [languages, classes] = (panel.components ?? []).map((row) => ("toJSON" in row ? row.toJSON() : row)) as any[];
  expect(languages.components).toHaveLength(LANGUAGES.length);
  expect(classes.components[0].options).toHaveLength(CLASSES.length + 1);
  const embed = (panel.embeds as any[])[0].toJSON();
  expect(embed.footer.text).toBe(WELCOME_PANEL);
  expect(embed.description).toContain("<#1>");
});

test("the support panel offers a ticket and a bug report", () => {
  const [row] = (supportPanel().components ?? []).map((r) => ("toJSON" in r ? r.toJSON() : r)) as any[];
  expect(row.components.map((c: any) => c.custom_id)).toEqual(["ticket:open", "bug:open"]);
});
