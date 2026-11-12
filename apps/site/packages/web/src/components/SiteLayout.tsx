import { Outlet } from "react-router-dom";
import { CallToAction } from "@/components/sections/CallToAction";
import { SiteFooter } from "@/components/SiteFooter";
import { SiteHeader } from "@/components/SiteHeader";

export function SiteLayout() {
  return (
    <div className="mx-auto w-full max-w-[1200px] border-x-2 border-divider bg-surface">
      <SiteHeader />
      <main>
        <Outlet />
        <CallToAction />
      </main>
      <SiteFooter />
    </div>
  );
}