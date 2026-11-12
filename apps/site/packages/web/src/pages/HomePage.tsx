import { BotCommandsSection } from "@/components/sections/BotCommandsSection";
import { CommunitySection } from "@/components/sections/CommunitySection";
import { DiscordSection } from "@/components/sections/DiscordSection";
import { FeaturesSection } from "@/components/sections/FeaturesSection";
import { Hero } from "@/components/sections/Hero";
import { HowToPlaySection } from "@/components/sections/HowToPlaySection";
import { StatsBar } from "@/components/sections/StatsBar";
import { TournamentBanner } from "@/components/sections/TournamentBanner";

export function HomePage() {
  return (
    <>
      <Hero />
      <TournamentBanner />
      <StatsBar />
      <CommunitySection />
      <DiscordSection />
      <HowToPlaySection />
      <FeaturesSection />
      <BotCommandsSection />
    </>
  );
}