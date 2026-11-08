import { Container } from "@needle-di/core";
import { DummyLadderRepository } from "./features/ladder/DummyLadderRepository";
import { LadderApi } from "./features/ladder/LadderApi";
import { LADDER_REPOSITORY } from "./features/ladder/LadderRepository";
import { LadderService } from "./features/ladder/LadderService";
import { MySqlLadderRepository } from "./features/ladder/MySqlLadderRepository";
import { CONFIG, loadConfig } from "./shared/Config";
import { createDb } from "./shared/Db";
import { HttpServer } from "./shared/HttpServer";
import { OnDeinit } from "./shared/OnDeinit";
import { OnInit } from "./shared/OnInit";

export function createContainer(env: NodeJS.ProcessEnv = process.env): Container {
  const container = new Container();
  const config = loadConfig(env);
  container.bind({ provide: CONFIG, useValue: config });

  container.bind(LadderService);
  container.bind(LadderApi);
  container.bind(HttpServer);

  // The ladder data source is chosen once at startup. Local development uses fixtures by default
  // (DATA_SOURCE=dummy), so frontend work needs no database.
  if (config.dataSource === "mysql") {
    const { db, close } = createDb(config.characterDbUrl, config.dbTimeoutMs);
    container.bind({
      provide: LADDER_REPOSITORY,
      useValue: new MySqlLadderRepository(db, config.dbTimeoutMs),
    });
    container.bind({ multi: true, provide: OnDeinit, useValue: { deinit: close } });
  } else {
    container.bind({ provide: LADDER_REPOSITORY, useValue: new DummyLadderRepository() });
  }

  container.bind({ multi: true, provide: OnInit, useExisting: HttpServer });
  container.bind({ multi: true, provide: OnDeinit, useExisting: HttpServer });

  return container;
}