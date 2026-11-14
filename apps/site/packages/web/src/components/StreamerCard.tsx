import { isFeaturedStreamer, type Streamer } from "@arenacraft/common";
import { Tag } from "@/components/Tag";
import { TwitchIcon } from "@/components/TwitchIcon";
import { formatUptime, formatViewers, watchUrl } from "@/lib/streamers";

/** A live channel card. Offline channels render as `OfflineStreamerRow` instead. */
export function StreamerCard({ streamer }: { streamer: Streamer }) {
  const live = streamer.live;
  if (live === null) return <OfflineStreamerRow streamer={streamer} />;

  return (
    <a
      href={watchUrl(streamer.login)}
      target="_blank"
      rel="noreferrer"
      className="group grid content-start border-2 border-divider transition-colors hover:border-ink/60"
    >
      <div className="relative aspect-video overflow-hidden bg-surface-raised">
        {live.thumbnailUrl === null ? (
          <div className="grid h-full place-items-center">
            <TwitchIcon className="size-10 text-ink/20" />
          </div>
        ) : (
          <img
            src={live.thumbnailUrl}
            alt=""
            loading="lazy"
            className="h-full w-full object-cover transition-transform duration-300 group-hover:scale-[1.03]"
          />
        )}

        <div className="absolute inset-x-0 top-0 flex items-center gap-2 p-2">
          <span className="bg-accent px-2 py-1 text-[11px] leading-none font-extrabold tracking-[0.08em] text-white uppercase">
            Live
          </span>
          <span className="bg-surface/90 px-2 py-1 text-[11px] leading-none font-bold">
            {formatViewers(live.viewers)} viewers
          </span>
        </div>
      </div>

      <div className="grid gap-2 p-3">
        <div className="flex items-center gap-2">
          <StreamerAvatar streamer={streamer} />
          <div className="min-w-0">
            <p className="truncate text-sm leading-[1.2] font-extrabold">{streamer.name}</p>
            <p className="truncate text-[13px] leading-[1.4] text-ink/60">
              {live.game} · live {formatUptime(live.startedAt, Date.now())}
            </p>
          </div>
          {isFeaturedStreamer(streamer) && (
            <span className="ml-auto shrink-0">
              <Tag>Arenacraft</Tag>
            </span>
          )}
        </div>

        <p className="line-clamp-2 text-[15px] leading-[1.45] font-bold">{live.title}</p>
      </div>
    </a>
  );
}

export function OfflineStreamerRow({ streamer }: { streamer: Streamer }) {
  return (
    <a
      href={watchUrl(streamer.login)}
      target="_blank"
      rel="noreferrer"
      className="flex items-center gap-3 border-b border-divider px-4 py-3 transition-colors last:border-b-0 hover:bg-surface-raised"
    >
      <StreamerAvatar streamer={streamer} dimmed />
      <span className="text-sm font-bold">{streamer.name}</span>
      <span className="text-[13px] text-ink/60">Offline</span>
      <TwitchIcon className="ml-auto size-4 text-ink/40" />
    </a>
  );
}

function StreamerAvatar({ streamer, dimmed = false }: { streamer: Streamer; dimmed?: boolean }) {
  if (streamer.avatarUrl === null) {
    return (
      <span
        className={`grid size-9 shrink-0 place-items-center bg-accent text-sm font-extrabold text-surface ${dimmed ? "opacity-60" : ""}`}
      >
        {streamer.name.slice(0, 1).toUpperCase()}
      </span>
    );
  }

  return (
    <img
      src={streamer.avatarUrl}
      alt=""
      loading="lazy"
      className={`size-9 shrink-0 object-cover ${dimmed ? "opacity-60" : ""}`}
    />
  );
}
