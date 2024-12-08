"use client";

import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Checkbox } from "@/components/ui/checkbox";
import { Progress } from "@/components/ui/progress";
import {
  Download,
  Play,
  Trash2,
  RefreshCw,
  SwordsIcon,
  BellPlus,
} from "lucide-react";
import { Alert, AlertDescription, AlertTitle } from "@/components/ui/alert";

export default function WoWUpdater() {
  const [isLoading, setIsLoading] = useState(false);
  const [isInstalled, setIsInstalled] = useState<boolean | null>(true);
  const [hasUpdates, setHasUpdates] = useState(false);

  function handlePlay() {}
  function handleClickOnCheckForUpdates() {}
  function handleClickOnDownloadAndApplyUpdates() {}
  function handleClickOnInstall() {}
  function handleClickOnRemove() {}

  const baseEnable = isLoading === false && isInstalled !== null;
  const enablePlay = baseEnable && isInstalled === true && hasUpdates === false;
  const enableCheckForUpdates = baseEnable && isInstalled === true;
  const enableDownloadAndApplyUpdates =
    baseEnable && isInstalled === true && hasUpdates === true;
  const enableInstall = baseEnable && isInstalled === false;
  const enableRemove = baseEnable && isInstalled === true;

  return (
    <div className="flex items-center justify-center min-h-screen bg-gray-100">
      <Card className="w-[640px] h-[480px]">
        <CardHeader>
          <CardTitle className="text-xl">ArenaCraft Launcher</CardTitle>
        </CardHeader>
        <CardContent className="flex flex-col">
          <div className="flex flex-row items-center gap-4 mb-4">
            <Progress className="" value={10} />
            <span className="text-sm font-semibold">10%</span>
          </div>
          {hasUpdates && isInstalled && <UpdatesAlert />}

          <div className="flex flex-col gap-2">
            <Button
              disabled={!enablePlay}
              variant={"default"}
              onClick={handlePlay}
            >
              <Play /> Play
            </Button>

            <Button
              onClick={handleClickOnCheckForUpdates}
              disabled={!enableCheckForUpdates}
            >
              <RefreshCw /> Check for Updates
            </Button>
            <Button
              disabled={!enableDownloadAndApplyUpdates}
              variant={"default"}
              onClick={handleClickOnDownloadAndApplyUpdates}
            >
              <Download /> Download & Apply Updates
            </Button>
            <Button
              onClick={handleClickOnInstall}
              variant={"primary"}
              disabled={!enableInstall}
            >
              <SwordsIcon /> Install ArenaCraft mods
            </Button>
            <Button
              variant={"destructive"}
              onClick={handleClickOnRemove}
              disabled={!enableRemove}
            >
              <Trash2 /> Remove ArenaCraft mods   
            </Button>
          </div>
          <AutoCheckUpdateCheckbox disabled={!baseEnable} />
        </CardContent>
      </Card>
    </div>
  );
}

function AutoCheckUpdateCheckbox(props: { disabled?: boolean }) {
  return (
    <div className="flex items-center space-x-2 mt-6 focus-within:outline focus-within:outline-2 focus-within:outline-pink-600 focus-within:outline-offset-2">
      <Checkbox id="terms" disabled={props.disabled} />
      <label
        htmlFor="terms"
        className="text-sm font-medium leading-none peer-disabled:cursor-not-allowed peer-disabled:opacity-70 "
      >
        Automatically check for updates during startup (recommended)
      </label>
    </div>
  );
}

export function UpdatesAlert() {
  return (
    <Alert className="mt-2 mb-2">
      <BellPlus className="h-4 w-4" />
      <AlertTitle className="font-semibold">Updates found</AlertTitle>
      <AlertDescription>
        Press the <em>"Download & Apply Updates"</em> button to get the latest
        version
      </AlertDescription>
    </Alert>
  );
}
