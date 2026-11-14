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
import { DummyPlayerOverlayRepository } from "./features/playerOverlay/DummyPlayerOverlayRepository";
import { MySqlPlayerOverlayRepository } from "./features/playerOverlay/MySqlPlayerOverlayRepository";
import { PlayerOverlayApi } from "./features/playerOverlay/PlayerOverlayApi";
import { PLAYER_OVERLAY_REPOSITORY } from "./features/playerOverlay/PlayerOverlayRepository";
import { PlayerOverlayService } from "./features/playerOverlay/PlayerOverlayService";
import { DummyStreamerStatusSource } from "./features/streamers/DummyStreamerStatusSource";
import { STREAMER_STATUS_SOURCE } from "./features/streamers/StreamerStatusSource";
import { StreamersApi } from "./features/streamers/StreamersApi";
import { StreamersService } from "./features/streamers/StreamersService";
import {
  TwitchStreamerStatusSource,
  type TwitchCredentials,
} from "./features/streamers/TwitchStreamerStatusSource";
import { CONFIG, loadConfig, type Config } from "./shared/Config";
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
  container.bind(PlayerOverlayService);
  container.bind(PlayerOverlayApi);
  container.bind(StreamersService);
  container.bind(StreamersApi);
  container.bind(HttpServer);

  // The streamer zone talks to Twitch in production and serves fixtures with DATA_SOURCE=dummy,
  // so the listing can be built without Twitch credentials. The interface has exactly these two
  // implementations; production without credentials lists every channel as offline.
  container.bind({
    provide: STREAMER_STATUS_SOURCE,
    useValue:
      config.dataSource === "dummy"
        ? new DummyStreamerStatusSource()
        : new TwitchStreamerStatusSource(twitchCredentials(config)),
  });

  // The ladder data source is chosen once at startup. Local development uses fixtures by default
  // (DATA_SOURCE=dummy), so frontend work needs no database.
  if (config.dataSource === "mysql") {
    const { db, close } = createDb(config.characterDbUrl, config.dbTimeoutMs);
    container.bind({
      provide: LADDER_REPOSITORY,
      useValue: new MySqlLadderRepository(db, config.dbTimeoutMs),
    });
    container.bind({ multi: true, provide: OnDeinit, useValue: { deinit: close } });

    // Aliases and overlay tokens live in the auth database. It is optional: without AUTH_DB_URL the
    // live ladder simply shows character names and personal overlay links do not resolve.
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
    container.bind({
      provide: PLAYER_OVERLAY_REPOSITORY,
      useValue: new MySqlPlayerOverlayRepository(db, authDb, config.dbTimeoutMs),
    });
  } else {
    container.bind({ provide: LADDER_REPOSITORY, useValue: new DummyLadderRepository() });
    container.bind({ provide: LADDER_TOP_SERVICE, useClass: MockLadderTopService });
    container.bind({
      provide: PLAYER_OVERLAY_REPOSITORY,
      useValue: new DummyPlayerOverlayRepository(),
    });
  }

  container.bind({ multi: true, provide: OnInit, useExisting: HttpServer });
  container.bind({ multi: true, provide: OnDeinit, useExisting: HttpServer });

  return container;
}

function twitchCredentials(config: Config): TwitchCredentials | null {
  if (config.twitchClientId === null || config.twitchClientSecret === null) return null;
  return { clientId: config.twitchClientId, clientSecret: config.twitchClientSecret };
}