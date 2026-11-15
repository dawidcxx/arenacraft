import { Route, Routes } from "react-router-dom";
import { SiteLayout } from "@/components/SiteLayout";
import { ArmoryCharacterPage } from "@/pages/ArmoryCharacterPage";
import { AddonsPage } from "@/pages/AddonsPage";
import { ArmoryPage } from "@/pages/ArmoryPage";
import { HallOfFamePage } from "@/pages/HallOfFamePage";
import { HomePage } from "@/pages/HomePage";
import { LadderPage } from "@/pages/LadderPage";
import { LiveLadderPage } from "@/pages/LiveLadderPage";
import { LiveLadderTourneyPage } from "@/pages/LiveLadderTourneyPage";
import { PlayerOverlayPage } from "@/pages/PlayerOverlayPage";
import { StreamerZonePage } from "@/pages/StreamerZonePage";
import { TourneyBannerPage } from "@/pages/TourneyBannerPage";
import { TournamentsPage } from "@/pages/TournamentsPage";

export function App() {
  return (
    <Routes>
      {/* Unlinked OBS widget with its own chrome-less layout. */}
      <Route path="live-ladder" element={<LiveLadderPage />} />
      {/* Unlinked OBS variant: live top 5 plus the tourney clock. */}
      <Route path="live-ladder-tourney" element={<LiveLadderTourneyPage />} />
      {/* Unlinked full-frame OBS cover for the tourney stream. */}
      <Route path="tourney-banner" element={<TourneyBannerPage />} />
      {/* Personal OBS overlay; the token comes from the Discord bot's /overlay command. */}
      <Route path="overlay/:token" element={<PlayerOverlayPage />} />

      <Route element={<SiteLayout />}>
        <Route index element={<HomePage />} />
        <Route path="tournaments" element={<TournamentsPage />} />
        <Route path="ladder" element={<LadderPage />} />
        <Route path="streamers" element={<StreamerZonePage />} />
        <Route path="armory" element={<ArmoryPage />} />
        <Route path="armory/:name" element={<ArmoryCharacterPage />} />
        <Route path="addons" element={<AddonsPage />} />
        <Route path="hall-of-fame" element={<HallOfFamePage />} />
        <Route path="*" element={<HomePage />} />
      </Route>
    </Routes>
  );
}