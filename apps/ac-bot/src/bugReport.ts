import {
  ActionRowBuilder,
  ChannelType,
  EmbedBuilder,
  MessageFlags,
  ModalBuilder,
  SlashCommandBuilder,
  TextInputBuilder,
  TextInputStyle,
} from "discord.js";
import type { ForumChannel, ModalSubmitInteraction } from "discord.js";

import { logger } from "./logger";
import { BUG_FORUM, CLASSES } from "./serverLayout";

// Bug reports: /bug (or the button in #support) opens a form; the answer becomes a post in the bug-reports forum,
// tagged with the class it is about and "Open". Every report is also handed to a BugReportSink - today that only
// logs; a GitHub sink (one issue per report) can be plugged in later without touching the form.

export const BUG_MODAL = "bug:submit";

export const BUG_COMMAND = new SlashCommandBuilder()
  .setName("bug")
  .setDescription("Report a bug - opens a short form")
  .setDMPermission(false)
  .toJSON();

export interface BugReport {
  title: string;
  classText: string;
  what: string;
  steps: string;
  expected: string;
  reporterId: string;
  reporterName: string;
}

export interface FiledBugReport extends BugReport {
  threadId: string;
  threadUrl: string;
  tags: string[];
}

export interface BugReportSink {
  submit(report: FiledBugReport): Promise<void>;
}

export const logBugReportSink: BugReportSink = {
  async submit(report) {
    logger.info(`Bug report "${report.title}" by ${report.reporterName}: ${report.threadUrl}`);
  },
};

const FIELD = { title: "title", classText: "class", what: "what", steps: "steps", expected: "expected" } as const;

export function buildBugModal(): ModalBuilder {
  const input = (id: string, label: string, style: TextInputStyle, required: boolean, placeholder: string, max: number) =>
    new ActionRowBuilder<TextInputBuilder>().addComponents(
      new TextInputBuilder()
        .setCustomId(id)
        .setLabel(label)
        .setStyle(style)
        .setRequired(required)
        .setPlaceholder(placeholder)
        .setMaxLength(max),
    );

  return new ModalBuilder()
    .setCustomId(BUG_MODAL)
    .setTitle("Report a bug")
    .addComponents(
      input(FIELD.title, "Short title", TextInputStyle.Short, true, "Blink stops on uneven floor", 90),
      input(FIELD.classText, "Class / spell (if any)", TextInputStyle.Short, false, "Mage - Blink", 100),
      input(FIELD.what, "What happened?", TextInputStyle.Paragraph, true, "What you saw, where, when", 1500),
      input(FIELD.steps, "How to reproduce", TextInputStyle.Paragraph, false, "1. ... 2. ... 3. ...", 1000),
      input(FIELD.expected, "What should happen instead?", TextInputStyle.Paragraph, false, "", 800),
    );
}

const CLASS_ALIASES: Record<string, string[]> = {
  deathknight: ["dk", "death knight", "deathknight"],
  warlock: ["warlock", "lock"],
  paladin: ["paladin", "pala", "pally"],
  priest: ["priest"],
  druid: ["druid"],
  hunter: ["hunter"],
  rogue: ["rogue"],
  mage: ["mage"],
  shaman: ["shaman", "sham"],
  warrior: ["warrior", "warr"],
};

/** Forum tags for a report: the class it names (if any, else "General") and "Open". */
export function guessTags(classText: string, available: readonly string[]): string[] {
  const words = classText.toLowerCase();
  const tags: string[] = [];
  for (const wowClass of CLASSES) {
    const aliases = CLASS_ALIASES[wowClass.key] ?? [wowClass.key];
    if (aliases.some((alias) => new RegExp(`\\b${alias}\\b`).test(words))) tags.push(wowClass.role);
  }
  if (tags.length === 0) tags.push("General");
  tags.push("Open");
  return tags.filter((tag) => available.includes(tag)).slice(0, 5); // Discord allows 5 tags per post
}

export function formatBugReport(report: BugReport): string {
  const section = (title: string, body: string) => (body.trim() ? `**${title}**\n${body.trim()}\n` : "");
  return [
    `Reported by <@${report.reporterId}>`,
    "",
    section("Class / spell", report.classText),
    section("What happened", report.what),
    section("How to reproduce", report.steps),
    section("Expected", report.expected),
  ]
    .filter((part) => part !== "")
    .join("\n")
    .slice(0, 2000);
}

export async function handleBugModal(interaction: ModalSubmitInteraction, sink: BugReportSink): Promise<void> {
  const guild = interaction.guild;
  const forum = guild?.channels.cache.find(
    (channel): channel is ForumChannel => channel.type === ChannelType.GuildForum && channel.name === BUG_FORUM,
  );
  if (!forum) {
    await interaction.reply({ content: "The bug forum is not set up yet - please post in #support.", flags: MessageFlags.Ephemeral });
    return;
  }
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  const field = (id: string) => interaction.fields.getTextInputValue(id) ?? "";
  const report: BugReport = {
    title: field(FIELD.title).trim() || "Bug report",
    classText: field(FIELD.classText),
    what: field(FIELD.what),
    steps: field(FIELD.steps),
    expected: field(FIELD.expected),
    reporterId: interaction.user.id,
    reporterName: interaction.user.username,
  };

  const tagNames = guessTags(report.classText, forum.availableTags.map((tag) => tag.name));
  const appliedTags = forum.availableTags.filter((tag) => tagNames.includes(tag.name)).map((tag) => tag.id);

  const thread = await forum.threads.create({
    name: report.title.slice(0, 100),
    appliedTags,
    message: { content: formatBugReport(report), allowedMentions: { users: [report.reporterId] } },
  });

  await interaction.editReply({
    embeds: [
      new EmbedBuilder()
        .setColor(0x2ecc71)
        .setDescription(`Thanks! Your report is here: <#${thread.id}>. Add screenshots or videos in that post.`),
    ],
  });

  try {
    await sink.submit({ ...report, threadId: thread.id, threadUrl: thread.url, tags: tagNames });
  } catch (error) {
    logger.error(`Bug report sink failed for ${thread.id}`, error);
  }
}
