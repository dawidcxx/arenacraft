import { type Streamer } from "@arenacraft/common";
import { PageHero } from "@/components/PageHero";
import { OfflineStreamerRow, StreamerCard } from "@/components/StreamerCard";
import { TwitchIcon } from "@/components/TwitchIcon";
import { useStreamers } from "@/hooks/useStreamers";
import { offlineStreamers, onlineStreamers, type StreamersState } from "@/lib/streamers";

export function StreamerZonePage() {
  const state = useStreamers();
  const streamers = state.status === "ready" ? state.streamers : [];
  const online = onlineStreamers(streamers);
  const offline = offlineStreamers(streamers);

  return (
    <>
      <PageHero eyebrow="Live on Twitch" title="Streamer zone" />

      <section className="grid border-b-2 border-divider md:grid-cols-2">
        <div className="border-b-2 border-divider p-4 md:border-r-2 md:border-b-0">
          <p className="text-lg leading-[1.5]">
            Watch Arenacraft players live on Twitch. Learn from the top of the ladder, catch tourney
            games and say hi in chat.
          </p>
        </div>

        <div className="flex flex-wrap items-center gap-2 p-4">
          <a
            href="https://www.twitch.tv/search?term=arenacraft"
            className="inline-flex items-center gap-2.5 bg-[#9146ff] px-3.5 py-2 text-sm font-extrabold text-white transition-[filter] hover:brightness-110"
          >
            <TwitchIcon className="size-[18px]" />
            Find Arenacraft on Twitch
          </a>
          <span className="text-sm">
            Streaming Arenacraft? Post your channel in Discord to get listed.
          </span>
        </div>
      </section>

      <section className="border-b-2 border-divider">
        <div className="flex flex-wrap items-baseline gap-x-3 gap-y-1 border-b-2 border-divider px-4 py-3">
          <h2 className="text-[28px] leading-[1.55] font-extrabold">Live now</h2>
          {state.status === "ready" && online.length > 0 && (
            <span className="text-sm text-ink/60">
              {online.length} {online.length === 1 ? "channel" : "channels"}
            </span>
          )}
          <span className="ml-auto text-[13px] text-ink/60">
            Twitch status refreshes every minute
          </span>
        </div>

        <LiveSection state={state} online={online} total={streamers.length} />
      </section>

      {offline.length > 0 && (
        <section className="border-b-2 border-divider">
          <div className="border-b-2 border-divider px-4 py-3">
            <h2 className="text-[28px] leading-[1.55] font-extrabold">Affiliated streamers</h2>
            <p className="text-sm text-ink/60">
              Offline right now. Follow them so you catch the next session.
            </p>
          </div>
          <div>
            {offline.map((streamer) => (
              <OfflineStreamerRow key={streamer.login} streamer={streamer} />
            ))}
          </div>
        </section>
      )}
    </>
  );
}

function LiveSection({
  state,
  online,
  total,
}: {
  state: StreamersState;
  online: Streamer[];
  total: number;
}) {
  if (state.status === "loading") {
    return <Notice>Checking who is live on Twitch…</Notice>;
  }

  if (state.status === "error") {
    return <Notice>{state.message}</Notice>;
  }

  if (total === 0) {
    return (
      <div className="grid gap-2 px-4 py-8">
        <h2 className="text-[28px] leading-[1.55] font-extrabold">No streamers listed yet.</h2>
        <p className="text-[15px] leading-[1.55]">
          Be the first. Go live on Twitch and drop your channel in our Discord.
        </p>
      </div>
    );
  }

  if (online.length === 0) {
    return <Notice>None of our streamers are live right now. The affiliate list is below.</Notice>;
  }

  return (
    <div className="grid gap-2 p-4 sm:grid-cols-2 xl:grid-cols-3">
      {online.map((streamer) => (
        <StreamerCard key={streamer.login} streamer={streamer} />
      ))}
    </div>
  );
}

function Notice({ children }: { children: string }) {
  return <p className="px-4 py-8 text-[15px] leading-[1.55] text-ink/70">{children}</p>;
}
