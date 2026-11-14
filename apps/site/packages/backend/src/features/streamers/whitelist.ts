/**
 * The affiliated Twitch channels shown on `/streamers`. There is no database table: this list is
 * the source of truth and ships with the code, like every other server-side rule of ours.
 *
 * `login` is the lowercase Twitch channel name (the part after `twitch.tv/`). `name` is only a
 * fallback display name used when Twitch is unavailable or has no profile for the channel; the
 * Twitch API supplies the real one otherwise.
 */
export interface WhitelistedStreamer {
  login: string;
  name: string;
}

export const STREAMER_WHITELIST: readonly WhitelistedStreamer[] = [
  { login: "meeeow1ng", "name": "Meowing" },
  { login: "fnobz", "name": "Fnobz" },
  { login: "lordofdiscx", "name": "LordOfDisc" },
  { login: "sajkelolx", name: "Sajke" },
  { login: "hvckfy", name: "Heckfy" },
  { login: "dawidpl95", name: "Diel" },
  { login: "filip_pl_", name: "FilipPL" },
  { login: "brooxz", name: "Brooxz" },
  { login: "nitroshield_", name: "Nitroshield" },
  { login: "homerjay_tv", name: "Homerjay" },
  { login: "kerosineyo", name: "Kerosine" },
  { login: "kerosineyo", name: "Kerosine" },
  { login: "shyxy", name: "Shyxy" },
];
