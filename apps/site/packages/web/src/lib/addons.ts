// Addons hosted by the site. The zips live in `packages/web/public/addons/` and are produced by
// `scripts/pack-addons` from the uploads in `apps/site/addons/` (git-ignored).

export type AddonCommand = { command: string; description: string };

export type Addon = {
  id: string;
  name: string;
  /** Null when the addon does not name its author. */
  author: string | null;
  version: string | null;
  /** Path of the zip under the site root. */
  download: string;
  /** Folders the zip contains; each goes directly under `Interface/AddOns`. */
  folders: readonly string[];
  summary: string;
  /** Highlights, shown as a list under the summary. */
  features?: readonly string[];
  commands: readonly AddonCommand[];
  credits?: string;
};

export type FeaturedAddon = Addon & { author: string; version: string };

export type AddonCategory = { title: string; addons: readonly Addon[] };

export const FEATURED_ADDON: FeaturedAddon = {
  id: "arenacraft-spectator",
  name: "Arenacraft Spectator",
  author: "hvckfy",
  version: "27",
  download: "/addons/ArenacraftSpectator.zip",
  folders: ["ArenacraftSpectator"],
  summary:
    "The spectator UI for Arenacraft. Watch live arenas and recorded replays with big player frames, cast bars, cooldowns, auras and a match timer, plus a timeline bar to pause and seek replays.",
  features: [
    "Live arena spectating with player frames, health, power, casts, cooldowns and auras",
    "Arena timer and combat feedback (damage and healing numbers)",
    "Replay timeline: play, pause, skip 10 seconds and a draggable seek bar",
    "Tournament mode with team names and a scoreboard for casting",
  ],
  commands: [
    { command: ".spect spectate <name>", description: "Watch the live arena a player is in" },
    { command: ".spect watch <name>", description: "Follow another player of the arena you spectate" },
    { command: ".spect leave", description: "Stop spectating" },
    { command: ".replay <id>", description: "Watch a recorded match (ids are on armory pages)" },
    { command: "/replaybar", description: "Show or hide the replay timeline" },
    { command: "/tournament on|off", description: "Toggle the tournament scoreboard" },
    { command: "/teamname1 <name>, /teamname2 <name>", description: "Set the team names on the scoreboard" },
    { command: "/score <1|2> <score>, /resetscore", description: "Set or reset a team's score" },
    { command: "/teamswitch", description: "Swap the two teams' names and scores" },
  ],
  credits: "Based on Sunwell AS by Kerhong and pussywizard; TidyPlates credit to Binny the Love Fool.",
};

export const ADDON_CATEGORIES: readonly AddonCategory[] = [
  {
    title: "Arena frames and enemy cooldowns",
    addons: [
      {
        id: "gladius",
        name: "Gladius Enhanced",
        author: "Proditor, Rinu, enhanced by Cortes",
        version: "1.2.2",
        download: "/addons/Gladius.zip",
        folders: ["Gladius"],
        summary:
          "Arena enemy unit frames: health, power, cast bars, specs, important auras and PvP trinket cooldowns for arena1-5.",
        commands: [
          { command: "/gladius", description: "Open the options" },
          { command: "/gladius test", description: "Show test frames (test2, test3 for smaller teams)" },
          { command: "/gladius hide", description: "Hide the test frames" },
        ],
      },
      {
        id: "omnibar",
        name: "OmniBar",
        author: "Jordon, backported to 3.3.5a by Jammin",
        version: null,
        download: "/addons/Omnibar.zip",
        folders: ["OmniBar", "OmniBar_Options"],
        summary: "Tracks enemy cooldowns on a bar of icons, so you see when their trinket, kick or defensives are back.",
        commands: [{ command: "/ob, /omnibar", description: "Open the options" }],
      },
    ],
  },
  {
    title: "Cooldowns and crowd control",
    addons: [
      {
        id: "omnicc",
        name: "OmniCC",
        author: "Tuller",
        version: "3.0.2",
        download: "/addons/OmniCC.zip",
        folders: ["OmniCC", "OmniCC_Config"],
        summary: "Cooldown numbers on every action button, item and aura icon.",
        commands: [{ command: "/omnicc, /occ", description: "Open the options" }],
      },
      {
        id: "losecontrol",
        name: "LoseControl",
        author: "Kouri",
        version: "3.33",
        download: "/addons/LoseControl.zip",
        folders: ["LoseControl"],
        summary: "A big icon with the remaining duration of every stun, fear, root or silence on you.",
        commands: [
          { command: "/lc unlock", description: "Unlock the icons to move them" },
          { command: "/lc lock", description: "Lock them again" },
          { command: "/lc reset", description: "Reset positions and settings" },
        ],
      },
    ],
  },
  {
    title: "Nameplates",
    addons: [
      {
        id: "heckfyplates",
        name: "heckfyplates",
        author: "hvckfy",
        version: "3.3.5.26",
        download: "/addons/heckfyplates.zip",
        folders: ["heckfyplates"],
        summary:
          "Arena nameplates you can tune down to every unit. Works on any default 3.3.5a client, no patches needed.",
        features: [
          "Cast bars under enemy nameplates in arena",
          "Move and adjust any nameplate; show or hide shaman totems, pets and other NPCs",
          "Hide totem nameplates and keep only their icons: clickable, so make them huge and hit the totem you need",
          "Totem ticks tracker",
          '"Enemy GCD": see when a button is pressed and how long the global cooldown lasts',
          '"Fear threshold": track how much damage a fear can take before it breaks',
          "Nameplate combat tracker and HP amount on nameplates",
          "Show arena target + class name, class name only, or arena target + nickname instead of names",
        ],
        commands: [{ command: "/hp, /heckfyplates", description: "Open the options" }],
        credits: "Built on Saiket's VirtualPlates.",
      },
    ],
  },
  {
    title: "Interface and keybinds",
    addons: [
      {
        id: "moveanything",
        name: "MoveAnything",
        author: "Wagthaa (WotLK), after Skrag, Jason and Vincent",
        version: "11.b2",
        download: "/addons/MoveAnything.zip",
        folders: ["MoveAnything"],
        summary: "Move, scale and hide any frame of the default UI.",
        commands: [
          { command: "/move", description: "Open the frame list" },
          { command: "/hide <frame>", description: "Hide a frame" },
        ],
      },
      {
        id: "actionbarsaver",
        name: "Action Bar Saver",
        author: "Shadowed",
        version: "2",
        download: "/addons/ActionBarSaver.zip",
        folders: ["ActionBarSaver"],
        summary: "Saves your action bars as named profiles and restores them, handy when you switch specs or rebuild a character.",
        commands: [
          { command: "/abs save <profile>", description: "Save the current bars" },
          { command: "/abs restore <profile>", description: "Load a saved profile" },
          { command: "/abs list", description: "List saved profiles" },
        ],
      },
      {
        id: "snowfallkeypress",
        name: "SnowfallKeyPress",
        author: null,
        version: null,
        download: "/addons/SnowfallKeyPress.zip",
        folders: ["SnowfallKeyPress", "SnowfallKeyPressSettings"],
        summary:
          "Key bindings fire when you press the key instead of when you release it, so every cast goes out a little sooner.",
        commands: [{ command: "Interface > AddOns", description: "Choose which keys are accelerated" }],
      },
    ],
  },
];

export const ADDON_INSTALL_STEPS: readonly string[] = [
  "Close World of Warcraft.",
  "Download the zip and extract it.",
  "Move every folder from the zip into World of Warcraft/Interface/AddOns (create the folder if it does not exist). Some addons come with two folders, e.g. OmniCC and OmniCC_Config.",
  "Make sure folders are not nested twice: the .toc file must sit at Interface/AddOns/<Addon>/<Addon>.toc.",
  'Start the game and enable the addons under "AddOns" on the character select screen. If an addon is marked out of date, tick "Load out of date AddOns".',
];
