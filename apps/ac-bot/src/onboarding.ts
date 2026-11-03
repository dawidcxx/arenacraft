import {
  ActionRowBuilder,
  ButtonBuilder,
  ButtonStyle,
  ChannelType,
  EmbedBuilder,
  GuildMember,
  MessageFlags,
  StringSelectMenuBuilder,
} from "discord.js";
import type {
  ButtonInteraction,
  Guild,
  MessageCreateOptions,
  Role,
  StringSelectMenuInteraction,
  TextChannel,
} from "discord.js";

import { logger } from "./logger";
import { BOT_CHANNEL, CLASSES, LANGUAGES, ROLE_DEVELOPER, ROLE_GM, ROLE_PLAYER, SUPPORT_CHANNEL } from "./serverLayout";

// Pinned panels the bot keeps up to date on startup, recognised by their embed footer.
export const WELCOME_PANEL = "arenacraft:welcome";
export const SUPPORT_PANEL = "arenacraft:support";

export const LANGUAGE_BUTTON = "lang:"; // + language code
export const CLASS_SELECT = "class:pick";
export const TICKET_BUTTON = "ticket:open";
export const BUG_BUTTON = "bug:open";

const NO_CLASS = "none";

export function welcomePanel(supportMention = `#${SUPPORT_CHANNEL}`): MessageCreateOptions {
  const embed = new EmbedBuilder()
    .setColor(0xc41f3b)
    .setTitle("Welcome to Arenacraft ⚔️")
    .setDescription(
      [
        "**1. Language** - pick yours below. It opens your language channel.",
        "**2. Account** - type `/register`: you get your login, password and realmlist (also by DM). " +
          "The rest of the server opens right after.",
        "**3. Class** - pick your main class below. You can change it any time here.",
        "",
        `Questions? Ask in ${supportMention} after registering.`,
      ].join("\n"),
    )
    .addFields(
      {
        name: "🇷🇺 Русский",
        value: "1. Выбери язык ниже. 2. Напиши `/register` - получишь логин, пароль и realmlist. 3. Выбери свой класс.",
      },
      {
        name: "🇩🇪 Deutsch",
        value: "1. Wähle unten deine Sprache. 2. Tippe `/register` - Login, Passwort und Realmlist. 3. Wähle deine Klasse.",
      },
      {
        name: "🇫🇷 Français",
        value: "1. Choisis ta langue. 2. Tape `/register` - identifiant, mot de passe et realmlist. 3. Choisis ta classe.",
      },
    )
    .setFooter({ text: WELCOME_PANEL });

  const languages = new ActionRowBuilder<ButtonBuilder>().addComponents(
    LANGUAGES.map((language) =>
      new ButtonBuilder()
        .setCustomId(`${LANGUAGE_BUTTON}${language.code}`)
        .setLabel(language.label)
        .setEmoji(language.emoji)
        .setStyle(ButtonStyle.Secondary),
    ),
  );

  const classes = new ActionRowBuilder<StringSelectMenuBuilder>().addComponents(
    new StringSelectMenuBuilder()
      .setCustomId(CLASS_SELECT)
      .setPlaceholder("Pick your main class")
      .addOptions(
        ...CLASSES.map((wowClass) => ({ label: wowClass.role, value: wowClass.key })),
        { label: "No class role", value: NO_CLASS, description: "Remove your class role" },
      ),
  );

  return { embeds: [embed], components: [languages, classes] };
}

export function supportPanel(): MessageCreateOptions {
  const embed = new EmbedBuilder()
    .setColor(0x3498db)
    .setTitle("Need help?")
    .setDescription(
      [
        "**🎫 Open a ticket** - a private thread with the staff: account problems, reports about players, anything " +
          "you don't want to post publicly.",
        "**🐞 Report a bug** - a short form; it becomes a post in the bug-reports forum where we track it.",
        "",
        "General questions are fine right here in the channel.",
      ].join("\n"),
    )
    .setFooter({ text: SUPPORT_PANEL });

  const buttons = new ActionRowBuilder<ButtonBuilder>().addComponents(
    new ButtonBuilder().setCustomId(TICKET_BUTTON).setLabel("Open a ticket").setEmoji("🎫").setStyle(ButtonStyle.Primary),
    new ButtonBuilder().setCustomId(BUG_BUTTON).setLabel("Report a bug").setEmoji("🐞").setStyle(ButtonStyle.Danger),
  );
  return { embeds: [embed], components: [buttons] };
}

function findTextChannel(guild: Guild, name: string): TextChannel | undefined {
  return guild.channels.cache.find(
    (channel): channel is TextChannel =>
      channel.type === ChannelType.GuildText && channel.name.toLowerCase() === name.toLowerCase(),
  );
}

/** Posts and pins the panel, or edits the pinned one the bot posted before (same footer). */
async function ensurePanel(guild: Guild, channelName: string, marker: string, panel: MessageCreateOptions) {
  const channel = findTextChannel(guild, channelName);
  if (!channel) {
    logger.warn(`#${channelName} not found, ${marker} panel not posted`);
    return;
  }
  const pinned = await channel.messages.fetchPinned();
  const existing = pinned.find(
    (message) => message.author.id === guild.client.user.id && message.embeds[0]?.footer?.text === marker,
  );
  if (existing) {
    await existing.edit({ embeds: panel.embeds, components: panel.components });
    return;
  }
  const message = await channel.send(panel);
  await message.pin();
  logger.info(`Posted the ${marker} panel in #${channelName}`);
}

export async function ensurePanels(guild: Guild): Promise<void> {
  const support = findTextChannel(guild, SUPPORT_CHANNEL);
  await ensurePanel(guild, BOT_CHANNEL, WELCOME_PANEL, welcomePanel(support ? `<#${support.id}>` : undefined));
  await ensurePanel(guild, SUPPORT_CHANNEL, SUPPORT_PANEL, supportPanel());
}

function role(guild: Guild, name: string): Role | undefined {
  return guild.roles.cache.find((r) => r.name.toLowerCase() === name.toLowerCase());
}

/** Gives the Player role (opens the server). Returns false when the role is missing or cannot be given. */
export async function grantPlayer(member: GuildMember): Promise<boolean> {
  const player = role(member.guild, ROLE_PLAYER);
  if (!player) return false;
  if (member.roles.cache.has(player.id)) return true;
  try {
    await member.roles.add(player, "Registered an Arenacraft account");
    return true;
  } catch (error) {
    logger.warn(`Could not give ${ROLE_PLAYER} to ${member.user.username}`, error);
    return false;
  }
}

export async function handleLanguageButton(interaction: ButtonInteraction): Promise<void> {
  const code = interaction.customId.slice(LANGUAGE_BUTTON.length);
  const language = LANGUAGES.find((l) => l.code === code);
  const member = interaction.member;
  if (!language || !(member instanceof GuildMember)) return;

  const languageRole = role(member.guild, language.role);
  if (!languageRole) {
    await interaction.reply({ content: "This language is not set up yet.", flags: MessageFlags.Ephemeral });
    return;
  }

  const had = member.roles.cache.has(languageRole.id);
  if (had) await member.roles.remove(languageRole, "Language picker");
  else await member.roles.add(languageRole, "Language picker");

  const registered = member.roles.cache.some((r) => r.name === ROLE_PLAYER);
  const next = registered ? "" : "\nNext: type `/register` to get your game account.";
  await interaction.reply({
    content: `${language.emoji} ${had ? "Removed" : "Added"} **${language.label}**. Click again to toggle.${next}`,
    flags: MessageFlags.Ephemeral,
  });
}

export async function handleClassSelect(interaction: StringSelectMenuInteraction): Promise<void> {
  const member = interaction.member;
  if (!(member instanceof GuildMember)) return;
  const picked = interaction.values[0];
  const wanted = CLASSES.find((wowClass) => wowClass.key === picked);

  const classRoles = CLASSES.map((wowClass) => role(member.guild, wowClass.role)).filter((r): r is Role => !!r);
  const toRemove = classRoles.filter((r) => member.roles.cache.has(r.id) && r.name !== wanted?.role);
  if (toRemove.length) await member.roles.remove(toRemove, "Class picker");

  if (!wanted) {
    await interaction.reply({ content: "Class role removed.", flags: MessageFlags.Ephemeral });
    return;
  }
  const wantedRole = role(member.guild, wanted.role);
  if (!wantedRole) {
    await interaction.reply({ content: "This class is not set up yet.", flags: MessageFlags.Ephemeral });
    return;
  }
  if (!member.roles.cache.has(wantedRole.id)) await member.roles.add(wantedRole, "Class picker");
  await interaction.reply({ content: `You are now a **${wanted.role}**.`, flags: MessageFlags.Ephemeral });
}

/** Opens (or points to) the member's private ticket thread in #support. */
export async function handleTicketButton(interaction: ButtonInteraction): Promise<void> {
  const guild = interaction.guild;
  const support = guild ? findTextChannel(guild, SUPPORT_CHANNEL) : undefined;
  if (!guild || !support) {
    await interaction.reply({ content: "Tickets are not available right now.", flags: MessageFlags.Ephemeral });
    return;
  }
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  const name = `ticket-${interaction.user.username}`.slice(0, 100);
  const active = await support.threads.fetchActive();
  const open = active.threads.find((thread) => thread.name === name && thread.parentId === support.id);
  if (open) {
    await interaction.editReply(`You already have an open ticket: <#${open.id}>`);
    return;
  }

  const thread = await support.threads.create({
    name,
    type: ChannelType.PrivateThread,
    invitable: false,
    reason: `Ticket for ${interaction.user.username}`,
  });
  await thread.members.add(interaction.user.id);
  const staff = [ROLE_GM, ROLE_DEVELOPER].map((name) => role(guild, name)).filter((r): r is Role => !!r);
  await thread.send(
    `${interaction.user} describe your problem here; only you and the staff see this thread.\n` +
      staff.map((r) => `${r}`).join(" "),
  );
  await interaction.editReply(`Ticket opened: <#${thread.id}>`);
  logger.info(`Opened ticket ${thread.id} for ${interaction.user.username}`);
}
