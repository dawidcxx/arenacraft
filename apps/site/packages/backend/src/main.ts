import { serializeError } from "serialize-error";
import { createContainer } from "./container";
import { Logger } from "./lib/Logger";
import { sleep } from "./lib/sleep";
import { OnDeinit } from "./shared/OnDeinit";
import { OnInit } from "./shared/OnInit";

const logger = Logger.for("main");
const container = createContainer();

process.title = "arenacraft";

try {
  const toInit: OnInit[] = container.get(OnInit, { multi: true });
  await Promise.all(toInit.map((initable) => initable.init()));
} catch (e) {
  logger.error("Failed to run init hooks", { error: serializeError(e) });
  process.exit(1);
}

function handleShutdown(signal: NodeJS.Signals) {
  logger.info("Shutdown signal retrieved", { signal });

  const toDeinit = container.get<OnDeinit>(OnDeinit, { multi: true });

  Promise.all(
    toDeinit.map(async (cleanupItem) => {
      try {
        await cleanupItem.deinit();
      } catch (e) {
        logger.error("Failed to deinit", {
          ctor: cleanupItem.constructor,
          error: serializeError(e),
        });
      }
    }),
  ).finally(() => {
    logger.info("Shutdown complete", { signal });
    process.exit(0);
  });
}

process.on("SIGINT", handleShutdown);
process.on("SIGTERM", handleShutdown);
process.on("unhandledRejection", (reason) => {
  logger.error("Unhandled promise rejection", { reason: serializeError(reason) });
  const onDone = () => logger.info("Exited due to unhandled rejection");
  Promise.race([sleep(500), handleShutdown("SIGTERM")]).then(onDone, onDone);
});