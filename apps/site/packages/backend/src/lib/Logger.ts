import { assertNever } from "@arenacraft/common";

let LOG_LEVEL: number = 0;

export const LOG_LEVELS = ["debug", "info", "warn", "error"] as const;
export type LogLevel = (typeof LOG_LEVELS)[number];

export class Logger {
  private context: string;

  private constructor(context: string) {
    this.context = context;
  }

  static for(ctorOrName: NewableFunction | string): Logger {
    return new Logger(typeof ctorOrName === "string" ? ctorOrName : ctorOrName.name);
  }

  static setLogLevel(level: LogLevel): void {
    switch (level) {
      case "debug":
        LOG_LEVEL = 10;
        break;
      case "info":
        LOG_LEVEL = 20;
        break;
      case "warn":
        LOG_LEVEL = 30;
        break;
      case "error":
        LOG_LEVEL = 40;
        break;
      default:
        assertNever(level);
    }
  }

  // log level = 10
  debug(msg: string, meta: LoggerMetaObj = {}): void {
    if (LOG_LEVEL <= 10) {
      this.writeLog("debug", msg, meta);
    }
  }

  // log level = 20
  info(msg: string, meta: LoggerMetaObj = {}): void {
    if (LOG_LEVEL <= 20) {
      this.writeLog("info", msg, meta);
    }
  }

  // log level = 30
  warn(msg: string, meta: LoggerMetaObj = {}): void {
    if (LOG_LEVEL <= 30) {
      this.writeLog("warn", msg, meta);
    }
  }

  // log level = 40
  error(msg: string, meta: LoggerMetaObj = {}): void {
    if (LOG_LEVEL <= 40) {
      this.writeLog("error", msg, meta);
    }
  }

  metric(message: string, value: number, unit: string): void {
    const line = JSON.stringify({
      level: "metric",
      message,
      unit,
      value,
    });
    console.log(line);
  }

  private writeLog(level: string, message: string, meta: LoggerMetaObj): void {
    const line = JSON.stringify({
      level,
      message,
      ts: new Date().toISOString(),
      meta,
      context: this.context,
    });
    console.log(line);
  }
}

export type LoggerMetaObj = Record<string, unknown> & {
  resourceId?: string | number | null;
};