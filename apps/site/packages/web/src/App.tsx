import { Route, Routes } from "react-router-dom";
import { SiteLayout } from "@/components/SiteLayout";
import { HomePage } from "@/pages/HomePage";
import { LadderPage } from "@/pages/LadderPage";
import { LiveLadderPage } from "@/pages/LiveLadderPage";
import { PlayerOverlayPage } from "@/pages/PlayerOverlayPage";
import { StreamerZonePage } from "@/pages/StreamerZonePage";
import { TournamentsPage } from "@/pages/TournamentsPage";

export function App() {
  return (
    <Routes>
      {/* Unlinked OBS widget with its own chrome-less layout. */}
      <Route path="live-ladder" element={<LiveLadderPage />} />
      {/* Personal OBS overlay; the token comes from the Discord bot's /overlay command. */}
      <Route path="overlay/:token" element={<PlayerOverlayPage />} />

      <Route element={<SiteLayout />}>
        <Route index element={<HomePage />} />
        <Route path="tournaments" element={<TournamentsPage />} />
        <Route path="ladder" element={<LadderPage />} />
        <Route path="streamers" element={<StreamerZonePage />} />
        <Route path="*" element={<HomePage />} />
      </Route>
    </Routes>
  );
}