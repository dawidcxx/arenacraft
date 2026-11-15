import { formatRemaining } from "@/lib/tourneyClock";

/** Tourney-day times are wall-clock CET/CEST, not the viewer's local zone. */
const EVENT_TIME_ZONE = "Europe/Berlin";

const CLOCK_FORMAT = new Intl.DateTimeFormat("en-GB", {
  timeZone: EVENT_TIME_ZONE,
  hour: "2-digit",
  minute: "2-digit",
  second: "2-digit",
  hourCycle: "h23",
});

const DAY_FORMAT = new Intl.DateTimeFormat("en-GB", {
  timeZone: EVENT_TIME_ZONE,
  weekday: "long",
  day: "numeric",
  month: "long",
});

export type EventClock = {
  /** `HH:MM:SS` in the event timezone. */
  label: string;
  /** Seconds since midnight in the event timezone. */
  secondsOfDay: number;
};

export function eventClock(now: Date): EventClock {
  const parts = CLOCK_FORMAT.formatToParts(now);
  const part = (type: Intl.DateTimeFormatPartTypes) =>
    Number(parts.find((entry) => entry.type === type)?.value ?? 0);

  return clockFromSecondsOfDay(part("hour") * 3600 + part("minute") * 60 + part("second"));
}

/** `HH:MM:SS` + seconds-since-midnight for a given second of the day (wraps at midnight). */
export function clockFromSecondsOfDay(secondsOfDay: number): EventClock {
  const normalized = ((Math.floor(secondsOfDay) % 86_400) + 86_400) % 86_400;
  const hours = Math.floor(normalized / 3600);
  const minutes = Math.floor((normalized % 3600) / 60);
  const seconds = normalized % 60;

  return {
    label: [hours, minutes, seconds].map((value) => String(value).padStart(2, "0")).join(":"),
    secondsOfDay: normalized,
  };
}

/** `Sunday 15 November` in the event timezone. */
export function eventDayLabel(date: Date): string {
  return DAY_FORMAT.format(date);
}

const secondsAt = (hour: number, minute: number) => hour * 3600 + minute * 60;

export type AgendaRow = {
  id: string;
  /** Time or time range as shown in the agenda. */
  range: string;
  title: string;
  startSecond: number;
  /** `null` for a checkpoint without a duration (the tourney start). */
  endSecond: number | null;
};

/** The stream agenda for a tourney day, in CET. */
export const TOURNEY_AGENDA: readonly AgendaRow[] = [
  {
    id: "stream-start",
    range: "17:45",
    title: "Stream start",
    startSecond: secondsAt(17, 45),
    endSecond: secondsAt(17, 59),
  },
  {
    id: "bracket-reset",
    range: "17:59",
    title: "Bracket reset",
    startSecond: secondsAt(17, 59),
    endSecond: secondsAt(18, 0),
  },
  {
    id: "tourney-start",
    range: "18:00",
    title: "Tourney start",
    startSecond: secondsAt(18, 0),
    endSecond: null,
  },
  {
    id: "gameplay",
    range: "18:00 – 20:15",
    title: "Gameplay stream",
    startSecond: secondsAt(18, 0),
    endSecond: secondsAt(20, 15),
  },
  {
    id: "commentary",
    range: "20:15 – 21:00",
    title: "Spectator commentary",
    startSecond: secondsAt(20, 15),
    endSecond: secondsAt(21, 0),
  },
];

export type AgendaStatus = "done" | "live" | "upcoming";

export function agendaStatus(row: AgendaRow, secondsOfDay: number): AgendaStatus {
  if (row.endSecond !== null && secondsOfDay >= row.startSecond && secondsOfDay < row.endSecond) {
    return "live";
  }
  if (secondsOfDay >= (row.endSecond ?? row.startSecond)) return "done";
  return "upcoming";
}

export type TourneyPhaseView = {
  kicker: string;
  /** Accent-filled kicker while the stream is running. */
  live: boolean;
  headline: string;
  caption: string;
  /** 0..1 through the running segment; `null` outside a timed segment. */
  progress: number | null;
};

/** Headline + countdown for the current moment of the tourney day. */
export function tourneyPhase(secondsOfDay: number): TourneyPhaseView {
  const streamStart = secondsAt(17, 45);
  const tourneyStart = secondsAt(18, 0);
  const gameplayEnd = secondsAt(20, 15);
  const commentaryEnd = secondsAt(21, 0);

  if (secondsOfDay < streamStart) {
    return {
      kicker: "Pre-show",
      live: false,
      headline: `Stream starts in ${formatRemaining(streamStart - secondsOfDay)}`,
      caption: "Tourney starts at 18:00",
      progress: null,
    };
  }

  if (secondsOfDay < tourneyStart) {
    return {
      kicker: "Stream live",
      live: true,
      headline: `Tourney starts in ${formatRemaining(tourneyStart - secondsOfDay)}`,
      caption: "Gameplay until 20:15",
      progress: (secondsOfDay - streamStart) / (tourneyStart - streamStart),
    };
  }

  if (secondsOfDay < gameplayEnd) {
    return {
      kicker: "Tourney live",
      live: true,
      headline: "Gameplay stream",
      caption: `Ends at 20:15 · ${formatRemaining(gameplayEnd - secondsOfDay)} left`,
      progress: (secondsOfDay - tourneyStart) / (gameplayEnd - tourneyStart),
    };
  }

  if (secondsOfDay < commentaryEnd) {
    return {
      kicker: "Tourney live",
      live: true,
      headline: "Spectator commentary",
      caption: `Ends at 21:00 · ${formatRemaining(commentaryEnd - secondsOfDay)} left`,
      progress: (secondsOfDay - gameplayEnd) / (commentaryEnd - gameplayEnd),
    };
  }

  return {
    kicker: "Post-show",
    live: false,
    headline: "Tourney finished",
    caption: "Thanks for watching — the next one is announced on Discord",
    progress: null,
  };
}
