import { Route, Routes } from "react-router-dom";
import { SiteLayout } from "@/components/SiteLayout";
import { HomePage } from "@/pages/HomePage";
import { LadderPage } from "@/pages/LadderPage";
import { StreamerZonePage } from "@/pages/StreamerZonePage";
import { TournamentsPage } from "@/pages/TournamentsPage";

export function App() {
  return (
    <Routes>
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