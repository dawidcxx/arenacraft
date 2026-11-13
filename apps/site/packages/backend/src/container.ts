import { Container } from "@needle-di/core";
import { DummyLadderRepository } from "./features/ladder/DummyLadderRepository";
import { LadderApi } from "./features/ladder/LadderApi";
import { LADDER_REPOSITORY } from "./features/ladder/LadderRepository";
import { LadderService } from "./features/ladder/LadderService";
import { MySqlLadderRepository } from "./features/ladder/MySqlLadderRepository";
import { LADDER_TOP_SERVICE } from "./features/liveLadder/ILadderTopService";
import { LADDER_TOP_REPOSITORY } from "./features/liveLadder/LadderTopRepository";
import { LadderTopService } from "./features/liveLadder/LadderTopService";
import { LadderTopSocket } from "./features/liveLadder/LadderTopSocket";
import { MockLadderTopService } from "./features/liveLadder/MockLadderTopService";
import { MySqlLadderTopRepository } from "./features/liveLadder/MySqlLadderTopRepository";
import { WebSocketConnectionLimiter } from "./features/liveLadder/WebSocketConnectionLimiter";
import { CONFIG, loadConfig } from "./shared/Config";
import { createDb, type DrizzleDb } from "./shared/Db";
import { HttpServer } from "./shared/HttpServer";
import { OnDeinit } from "./shared/OnDeinit";
import { OnInit } from "./shared/OnInit";

export function createContainer(env: NodeJS.ProcessEnv = process.env): Container {
  const container = new Container();
  const config = loadConfig(env);
  container.bind({ provide: CONFIG, useValue: config });

  container.bind(LadderService);
  container.bind(LadderApi);
  container.bind(WebSocketConnectionLimiter);
  container.bind(LadderTopSocket);
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

    // Aliases live in the auth database. They are optional: without AUTH_DB_URL the widget simply
    // shows character names.
    let authDb: DrizzleDb | null = null;
    if (config.authDbUrl !== null) {
      const auth = createDb(config.authDbUrl, config.dbTimeoutMs);
      authDb = auth.db;
      container.bind({ multi: true, provide: OnDeinit, useValue: { deinit: auth.close } });
    }

    container.bind({
      provide: LADDER_TOP_REPOSITORY,
      useValue: new MySqlLadderTopRepository(db, authDb, config.dbTimeoutMs),
    });
    container.bind({ provide: LADDER_TOP_SERVICE, useClass: LadderTopService });
  } else {
    container.bind({ provide: LADDER_REPOSITORY, useValue: new DummyLadderRepository() });
    container.bind({ provide: LADDER_TOP_SERVICE, useClass: MockLadderTopService });
  }

  container.bind({ multi: true, provide: OnInit, useExisting: HttpServer });
  container.bind({ multi: true, provide: OnDeinit, useExisting: HttpServer });

  return container;
}