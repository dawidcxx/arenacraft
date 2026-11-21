import records from "./tournaments.json";

/** Tourney-day times are wall-clock CET/CEST, not the viewer's local zone. */
const EVENT_TIME_ZONE = "Europe/Berlin";

const DAY_FORMAT = new Intl.DateTimeFormat("en-GB", {
  timeZone: EVENT_TIME_ZONE,
  weekday: "long",
  day: "numeric",
  month: "long",
});

const SHORT_DATE_FORMAT = new Intl.DateTimeFormat("en-GB", {
  timeZone: EVENT_TIME_ZONE,
  weekday: "short",
  day: "numeric",
  month: "short",
});

const TIME_FORMAT = new Intl.DateTimeFormat("en-GB", {
  timeZone: EVENT_TIME_ZONE,
  hour: "2-digit",
  minute: "2-digit",
  hourCycle: "h23",
});

const TIME_ZONE_FORMAT = new Intl.DateTimeFormat("en-GB", {
  timeZone: EVENT_TIME_ZONE,
  hour: "2-digit",
  minute: "2-digit",
  hourCycle: "h23",
  timeZoneName: "short",
});

export type TournamentStatus = "finished" | "upcoming";

export type TournamentPrize = {
  amount: string;
  description: string;
  highlight: boolean;
};

/**
 * A tourney on the calendar. Everything time-related is derived from `startsAt`/`endsAt`, so the
 * same record switches from upcoming to finished (and disappears from the upcoming list) on its
 * own. Add new tourneys to `tournaments.json`.
 */
export type Tournament = {
  id: string;
  number: number;
  title: string;
  description: string;
  format: string;
  startsAt: Date;
  endsAt: Date;
  /** `18:00–21:00, Sunday 8th of November (CET timezone)` */
  date: string;
  /** `18:00–21:00 CET` */
  windowLabel: string;
  /** `Sun 8 Nov, 18:00 CET` */
  shortDateLabel: string;
  prizes: TournamentPrize[];
};

type TournamentRecord = {
  id: string;
  number: number;
  title: string;
  description: string;
  format: string;
  startsAt: string;
  endsAt: string;
  prizes: TournamentPrize[];
};

function partOf(
  format: Intl.DateTimeFormat,
  type: Intl.DateTimeFormatPartTypes,
  date: Date,
): string {
  return format.formatToParts(date).find((part) => part.type === type)?.value ?? "";
}

function ordinal(day: number): string {
  const mod100 = day % 100;
  if (mod100 >= 11 && mod100 <= 13) return `${day}th`;

  switch (day % 10) {
    case 1:
      return `${day}st`;
    case 2:
      return `${day}nd`;
    case 3:
      return `${day}rd`;
    default:
      return `${day}th`;
  }
}

function dayLabel(date: Date): string {
  const day = Number(partOf(DAY_FORMAT, "day", date));
  return `${partOf(DAY_FORMAT, "weekday", date)} ${ordinal(day)} of ${partOf(DAY_FORMAT, "month", date)}`;
}

function timeLabel(date: Date): string {
  return TIME_FORMAT.format(date);
}

function timeZoneLabel(date: Date): string {
  return partOf(TIME_ZONE_FORMAT, "timeZoneName", date);
}

function toTournament(record: TournamentRecord): Tournament {
  const startsAt = new Date(record.startsAt);
  const endsAt = new Date(record.endsAt);
  const window = `${timeLabel(startsAt)}–${timeLabel(endsAt)}`;
  const timeZone = timeZoneLabel(startsAt);

  return {
    ...record,
    startsAt,
    endsAt,
    date: `${window}, ${dayLabel(startsAt)} (${timeZone} timezone)`,
    windowLabel: `${window} ${timeZone}`,
    shortDateLabel: `${SHORT_DATE_FORMAT.format(startsAt)}, ${timeLabel(startsAt)} ${timeZone}`,
  };
}

/** Every tourney on record, soonest first. */
export const TOURNAMENTS: readonly Tournament[] = (records as TournamentRecord[])
  .map(toTournament)
  .sort((a, b) => a.startsAt.getTime() - b.startsAt.getTime());

/** A tourney stays "upcoming" until its window closes. */
export function tournamentStatus(
  tournament: Tournament,
  now: Date = new Date(),
): TournamentStatus {
  return now.getTime() >= tournament.endsAt.getTime() ? "finished" : "upcoming";
}

/** Tourneys that have not finished yet, soonest first. */
export function upcomingTournaments(now: Date = new Date()): Tournament[] {
  return TOURNAMENTS.filter((tournament) => tournamentStatus(tournament, now) === "upcoming");
}

/** The next tourney to happen, if one is scheduled. */
export function nextTournament(now: Date = new Date()): Tournament | undefined {
  return upcomingTournaments(now)[0];
}

/** The most recently finished tourney, if any. */
export function latestFinishedTournament(now: Date = new Date()): Tournament | undefined {
  let latest: Tournament | undefined;

  for (const tournament of TOURNAMENTS) {
    if (tournamentStatus(tournament, now) === "finished") latest = tournament;
  }

  return latest;
}

export const TOURNAMENT_RULES = [
  "Players with a poor reputation are banned from participating. If you are curious if fall under this category DM me @Randomsheep 99,9% of players need not to worry about it",
  "Griefing is strictly prohibited. Expect harsh punishments",
  "We reserve the rights to adjust the rules at any moment",
  "Avoid toxic behavior towards lower skill players",
  "Joining Voice Chat, even if full mute is encouraged",
  "Lottery win chance scales with your play count, you need to complete 5 won games played to participate",
];
