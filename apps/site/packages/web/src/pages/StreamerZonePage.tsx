import { PageHero } from "@/components/PageHero";
import { TwitchIcon } from "@/components/TwitchIcon";

export function StreamerZonePage() {
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

      <section className="grid gap-2 border-b-2 border-divider px-4 py-8">
        <h2 className="text-[28px] leading-[1.55] font-extrabold">No streamers listed yet.</h2>
        <p className="text-[15px] leading-[1.55]">
          Be the first. Go live on Twitch and drop your channel in our Discord.
        </p>
      </section>
    </>
  );
}