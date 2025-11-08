"use client";

import { useState, useEffect, useCallback, useRef } from "react";
import { useRouter } from "next/navigation";
import { Button } from "@/components/ui/button";
import {
  Card,
  CardContent,
  CardDescription,
  CardFooter,
  CardHeader,
  CardTitle,
} from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { AppState, DriveDayState } from "@/lib/types";

export default function DrivedayPage() {
  const router = useRouter();
  const [clientId] = useState(() => crypto.randomUUID());
  const [appState, setAppState] = useState<AppState>({});
  const [driveDayState, setDriveDayState] = useState<DriveDayState>({});
  // Buffers to preserve in-progress decimal input like "12." or ".5"
  const [floatInput, setFloatInput] = useState<{
    airTemperature?: string;
    relativeHumidity?: string;
    trackTemperature?: string;
  }>({});

  const appStateRef = useRef(appState);
  appStateRef.current = appState;

  const sendStateUpdate = useCallback(
    async (newState: Partial<AppState>) => {
      const fullState = {
        ...appStateRef.current,
        ...newState,
        lastUpdatedBy: clientId,
      };
      try {
        await fetch("/api/event-sync", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(fullState),
        });
      } catch (error) {
        console.error("Failed to send state update:", error);
      }
    },
    [clientId]
  );

  useEffect(() => {
    const eventSource = new EventSource("/api/event-sync");
    eventSource.onmessage = (event) => {
      const newState: AppState = JSON.parse(event.data);
      setAppState(newState);
      if (newState.lastUpdatedBy !== clientId && newState.driveDay) {
        setDriveDayState(newState.driveDay);
        // refresh float text buffers only on remote updates
        setFloatInput({
          airTemperature:
            newState.driveDay.airTemperature != null
              ? String(newState.driveDay.airTemperature)
              : "",
          relativeHumidity:
            newState.driveDay.relativeHumidity != null
              ? String(newState.driveDay.relativeHumidity)
              : "",
          trackTemperature:
            newState.driveDay.trackTemperature != null
              ? String(newState.driveDay.trackTemperature)
              : "",
        });
      }
    };
    return () => eventSource.close();
  }, [clientId]);


  // Accept only digits and at most one decimal point for numeric-string inputs
  const numericStringAllowed = (s: string) => /^\d*(?:\.\d*)?$/.test(s);
  // Accept only integer digits (no decimal)
  const integerStringAllowed = (s: string) => /^\d*$/.test(s);

  // All validation is handled in onChange via regex; no key/paste guards needed

  const handleInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const { id } = e.target;
    const valueStr = e.target.value;

    const integerFields: Array<keyof DriveDayState> = ["powerLimit"];

    // Fields that are numbers in DriveDayState
    const floatFields: Array<keyof DriveDayState> = [
      "airTemperature",
      "relativeHumidity",
      "trackTemperature",
    ];

    const updated: DriveDayState = { ...driveDayState };

    if (integerFields.includes(id as keyof DriveDayState)) {
      if (valueStr === "") {
        (updated as any)[id] = undefined;
      } else if (integerStringAllowed(valueStr)) {
        (updated as any)[id] = parseInt(valueStr, 10);
      } else {
        return; // ignore invalid chars
      }
    } else if (floatFields.includes(id as keyof DriveDayState)) {
      if (valueStr === "") {
        // allow clearing
        (updated as any)[id] = undefined;
        setFloatInput((prev) => ({ ...prev, [id]: "" }));
      } else if (numericStringAllowed(valueStr)) {
        // always reflect raw text so a trailing '.' is preserved while typing
        setFloatInput((prev) => ({ ...prev, [id]: valueStr }));
        // only update numeric state when there is a digit present
        if (/[0-9]/.test(valueStr)) {
          const n = parseFloat(valueStr);
          if (Number.isFinite(n)) {
            (updated as any)[id] = n;
          }
        }
      } else {
        return;
      }
    } else {
      // default fallback: store raw string
      (updated as any)[id] = valueStr;
    }

    setDriveDayState(updated);
    sendStateUpdate({ driveDay: updated });
  };

  const handleCreate = async () => {
    const response = await fetch("/api/new-drive-day", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        power_limit: driveDayState.powerLimit,
        air_temperature: driveDayState.airTemperature,
        relative_humidity: driveDayState.relativeHumidity,
        track_temperature: driveDayState.trackTemperature,
      }),
    });
    if (response.status != 201) {
      console.error("Failed to create new drive day");
      return;
    }
    sendStateUpdate({ currentPage: "/event/new" });
    router.push("/event/new");
  };

  return (
    <div className="p-8 flex justify-center items-center pt-20">
      <Card className="w-[350px]">
        <CardHeader>
          <CardTitle>Create New Drive Day</CardTitle>
          <CardDescription>
            Enter the details for the new drive day.
          </CardDescription>
        </CardHeader>
        <CardContent>
          <form>
            <div className="grid w-full items-center gap-4">
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="powerLimit">Power Limit</Label>
                <Input
                  id="powerLimit"
                  type="text"
                  inputMode="numeric"
                  step={1}
                  placeholder="Enter power limit"
                  value={driveDayState.powerLimit ?? ""}
                  onChange={handleInputChange}
                  
                />
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="airTemperature">Air Temperature</Label>
                <Input
                  type="text"
                  id="airTemperature"
                  inputMode="decimal"
                  placeholder="Enter air temperature"
                  value={
                    floatInput.airTemperature ??
                    (driveDayState.airTemperature != null
                      ? String(driveDayState.airTemperature)
                      : "")
                  }
                  onChange={handleInputChange}
                   
                />
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="relativeHumidity">Relative Humidity</Label>
                <Input
                  type="text"
                  id="relativeHumidity"
                  inputMode="decimal"
                  placeholder="Enter relative humidity"
                  value={
                    floatInput.relativeHumidity ??
                    (driveDayState.relativeHumidity != null
                      ? String(driveDayState.relativeHumidity)
                      : "")
                  }
                  onChange={handleInputChange}
                  
                />
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="trackTemperature">Track Temperature</Label>
                <Input
                  type="text"
                  id="trackTemperature"
                  inputMode="decimal"
                  placeholder="Enter track temperature"
                  value={
                    floatInput.trackTemperature ??
                    (driveDayState.trackTemperature != null
                      ? String(driveDayState.trackTemperature)
                      : "")
                  }
                  onChange={handleInputChange}
                  
                />
              </div>
            </div>
          </form>
        </CardContent>
        <CardFooter className="flex justify-between">
          <Button variant="outline">Cancel</Button>
          <Button onClick={handleCreate}>Create</Button>
        </CardFooter>
      </Card>
    </div>
  );
}
