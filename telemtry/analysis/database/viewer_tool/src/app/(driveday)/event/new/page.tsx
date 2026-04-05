"use client";

import { useState, useEffect, useCallback, useRef } from "react";
import { useRouter } from "next/navigation";
import { Button } from "@/components/ui/button";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/ui/card";
import { Checkbox } from "@/components/ui/checkbox";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import { AppState, NewEventState } from "@/lib/types";

export default function NewEventPage() {
  const router = useRouter();
  const [clientId] = useState(() => crypto.randomUUID());
  const [appState, setAppState] = useState<AppState>({});
  const [newEventState, setNewEventState] = useState<NewEventState>({});

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
    if (appState.currentPage) {
      router.push(appState.currentPage);
    }
  }, [appState.currentPage, router]);

  useEffect(() => {
    const eventSource = new EventSource("/api/event-sync");
    eventSource.onmessage = (event) => {
      const newState: AppState = JSON.parse(event.data);
      setAppState(newState);
      if (newState.lastUpdatedBy !== clientId && newState.newEvent) {
        setNewEventState(newState.newEvent);
      }
    };
    return () => eventSource.close();
  }, [clientId]);

  // Prevent entering exponent/sign characters into numeric fields
  const blockInvalidNumberKeys = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (["e", "E", "+", "-"].includes(e.key)) e.preventDefault();
  };

  // Restrict numeric-string inputs to digits and an optional single decimal point
  const numericStringAllowed = (s: string) => /^\d*(?:\.\d*)?$/.test(s);

  const handleInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const { id } = e.target;
    const valueStr = e.target.value;

    // Fields stored as strings but should be numeric-only
    const numericStringKeys: Array<keyof NewEventState> = [
      // basics
      "carWeight",
      "powerLimit",
      "torqueLimit",
      "towAngle",
      // alignment
      "camberFront",
      "camberRear",
      "toeFront",
      "toeRear",
      // ride height
      "rideHeightFront",
      "rideHeightRear",
      // damping per corner
      "frLSC",
      "frLSR",
      "frHSC",
      "frHSR",
      "flLSC",
      "flLSR",
      "flHSC",
      "flHSR",
      "rrLSC",
      "rrLSR",
      "rrHSC",
      "rrHSR",
      "rlLSC",
      "rlLSR",
      "rlHSC",
      "rlHSR",
      // pressures
      "frPressure",
      "flPressure",
      "rrPressure",
      "rlPressure",
      // tires
      "frWearDepth",
      "flWearDepth",
      "rrWearDepth",
      "rlWearDepth",
      "frDurometer",
      "flDurometer",
      "rrDurometer",
      "rlDurometer",
      // aero
      "frontWingPitch",
      "rearWingPitch",
      // specialty
      "frontRollSpringRate",
      "frontHeaveSpringRate",
      "rearRollSpringRate",
      "rearHeaveSpringRate",
      // 2026 per-axle corner spring rate
      "frontCornerSpringRate",
      "rearCornerSpringRate",
    ];

    const next = { ...newEventState } as any;

    if (numericStringKeys.includes(id as keyof NewEventState)) {
      if (valueStr === "" || numericStringAllowed(valueStr)) {
        next[id] = valueStr;
      } else {
        return; // ignore invalid
      }
    } else {
      next[id] = valueStr;
    }

    setNewEventState(next);
    sendStateUpdate({ newEvent: next });
  };

  const handleCheckboxChange = (id: keyof NewEventState, checked: boolean) => {
    const newFormState = { ...newEventState, [id]: checked };
    setNewEventState(newFormState);
    sendStateUpdate({ newEvent: newFormState });
  };

  const handleSelectChange = (id: keyof NewEventState, value: string) => {
    // Fields that are numeric in NewEventState
    const numericKeys: Array<keyof NewEventState> = [
      "driverId",
      "locationId",
      "eventType",
      "carId",
    ];

    let parsedValue: any = value;
    if (numericKeys.includes(id)) {
      const n = Number(value);
      parsedValue = Number.isNaN(n) ? undefined : n;
    }

    const newFormState = { ...newEventState, [id]: parsedValue };
    setNewEventState(newFormState);
    sendStateUpdate({ newEvent: newFormState });
  };

  const handleEndDriveDay = async () => {
    await sendStateUpdate({ driveDay: undefined, newEvent: undefined, currentPage: "/driveday" });
    router.push("/driveday");
  };

  const handleCreateEvent = async () => {
    const response = await fetch("/api/create-event", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(newEventState),
    });
    if (response.status != 201) {
      console.error("Failed to create new event");
      return;
    }
    sendStateUpdate({ currentPage: "/event/in-progress" });
    router.push("/event/in-progress");
  };

  return (
    <div className="p-8 flex justify-center items-center pt-20">
      <Card className="w-full max-w-4xl">
        <CardHeader className="flex flex-row items-center justify-between">
          <div>
            <CardTitle>Create New Event</CardTitle>
            <CardDescription>
              Enter the details for the new event.
            </CardDescription>
          </div>
          <Button variant="outline" type="button" onClick={handleEndDriveDay}>
            End Drive Day
          </Button>
        </CardHeader>
        <CardContent>
          <form>
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="driverId">Choose a driver</Label>
                <Select
                  onValueChange={(v) => handleSelectChange("driverId", v)}
                  value={newEventState.driverId?.toString()}
                >
                  <SelectTrigger>
                    <SelectValue placeholder="Select a driver" />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectItem value="0">Other</SelectItem>
                    <SelectItem value="4">Andrew Cloran</SelectItem>
                    <SelectItem value="7">Viraj Bhalla</SelectItem>
                    <SelectItem value="8">Luke Ballengee</SelectItem>
                    <SelectItem value="5">Ali Jensen</SelectItem>
                  </SelectContent>
                </Select>
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="locationId">Choose a location</Label>
                <Select
                  onValueChange={(v) => handleSelectChange("locationId", v)}
                  value={newEventState.locationId?.toString()}
                >
                  <SelectTrigger>
                    <SelectValue placeholder="Select a location" />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectItem value="0">Other</SelectItem>
                    <SelectItem value="1">Pickle - Innovation Blvd</SelectItem>
                    <SelectItem value="2">Pickle - Front Lot</SelectItem>
                    <SelectItem value="3">Pickle - Other Lot</SelectItem>
                    <SelectItem value="4">COTA - Lot J</SelectItem>
                    <SelectItem value="5">COTA - Lot H</SelectItem>
                    <SelectItem value="6">COTA - Go Kart Track</SelectItem>
                  </SelectContent>
                </Select>
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="eventType">Choose an event type</Label>
                <Select
                  onValueChange={(v) => handleSelectChange("eventType", v)}
                  value={newEventState.eventType?.toString()}
                >
                  <SelectTrigger>
                    <SelectValue placeholder="Select an event type" />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectItem value="0">Other</SelectItem>
                    <SelectItem value="1">Endurance</SelectItem>
                    <SelectItem value="2">Autocross</SelectItem>
                    <SelectItem value="3">Skidpad</SelectItem>
                    <SelectItem value="4">
                      Straight Line Acceleration
                    </SelectItem>
                    <SelectItem value="5">Straight Line Breaking</SelectItem>
                  </SelectContent>
                </Select>
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="carId">Choose a car</Label>
                <Select
                  onValueChange={(v) => handleSelectChange("carId", v)}
                  value={newEventState.carId?.toString()}
                >
                  <SelectTrigger>
                    <SelectValue placeholder="Select a car" />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectItem value="1">Easy Driver</SelectItem>
                    <SelectItem value="2">Lady Luck</SelectItem>
                    <SelectItem value="3">Angelique</SelectItem>
                    <SelectItem value="4">Nightwatch</SelectItem>
                    <SelectItem value="5">Orion</SelectItem>
                  </SelectContent>
                </Select>
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="carWeight">Car Weight</Label>
                <Input
                  id="carWeight"
                  type="number"
                  inputMode="decimal"
                  value={newEventState.carWeight ?? ""}
                  onChange={handleInputChange}
                  onKeyDown={blockInvalidNumberKeys}
                />
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="towAngle">Tow Angle</Label>
                <Input
                  id="towAngle"
                  type="number"
                  inputMode="decimal"
                  value={newEventState.towAngle ?? ""}
                  onChange={handleInputChange}
                  onKeyDown={blockInvalidNumberKeys}
                />
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="camberFront">Camber (Front)</Label>
                <Input
                  id="camberFront"
                  type="number"
                  inputMode="decimal"
                  value={newEventState.camberFront ?? ""}
                  onChange={handleInputChange}
                  onKeyDown={blockInvalidNumberKeys}
                />
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="camberRear">Camber (Rear)</Label>
                <Input
                  id="camberRear"
                  type="number"
                  inputMode="decimal"
                  value={newEventState.camberRear ?? ""}
                  onChange={handleInputChange}
                  onKeyDown={blockInvalidNumberKeys}
                />
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="toeFront">Toe (Front)</Label>
                <Input
                  id="toeFront"
                  type="number"
                  inputMode="decimal"
                  value={newEventState.toeFront ?? ""}
                  onChange={handleInputChange}
                  onKeyDown={blockInvalidNumberKeys}
                />
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="toeRear">Toe (Rear)</Label>
                <Input
                  id="toeRear"
                  type="number"
                  inputMode="decimal"
                  value={newEventState.toeRear ?? ""}
                  onChange={handleInputChange}
                  onKeyDown={blockInvalidNumberKeys}
                />
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="rideHeightFront">Ride Height (Front)</Label>
                <Input
                  id="rideHeightFront"
                  type="number"
                  inputMode="decimal"
                  value={newEventState.rideHeightFront ?? ""}
                  onChange={handleInputChange}
                  onKeyDown={blockInvalidNumberKeys}
                />
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="rideHeightRear">Ride Height (Rear)</Label>
                <Input
                  id="rideHeightRear"
                  type="number"
                  inputMode="decimal"
                  value={newEventState.rideHeightRear ?? ""}
                  onChange={handleInputChange}
                  onKeyDown={blockInvalidNumberKeys}
                />
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="powerLimit">Power Limit</Label>
                <Input
                  id="powerLimit"
                  type="number"
                  inputMode="decimal"
                  value={newEventState.powerLimit ?? ""}
                  onChange={handleInputChange}
                  onKeyDown={blockInvalidNumberKeys}
                />
              </div>
              <div className="col-span-full grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
                <div className="flex flex-col space-y-1.5">
                  <Label>Front Right Shock Damping</Label>
                  <div className="grid grid-cols-2 gap-2">
                    <Input
                      id="frLSC"
                      placeholder="LSC"
                      type="number"
                      inputMode="numeric"
                      value={newEventState.frLSC ?? ""}
                      onChange={handleInputChange}
                      onKeyDown={blockInvalidNumberKeys}
                    />
                    <Input
                      id="frLSR"
                      placeholder="LSR"
                      type="number"
                      inputMode="numeric"
                      value={newEventState.frLSR ?? ""}
                      onChange={handleInputChange}
                      onKeyDown={blockInvalidNumberKeys}
                    />
                    <Input
                      id="frHSC"
                      placeholder="HSC"
                      type="number"
                      inputMode="numeric"
                      value={newEventState.frHSC ?? ""}
                      onChange={handleInputChange}
                      onKeyDown={blockInvalidNumberKeys}
                    />
                    <Input
                      id="frHSR"
                      placeholder="HSR"
                      type="number"
                      inputMode="numeric"
                      value={newEventState.frHSR ?? ""}
                      onChange={handleInputChange}
                      onKeyDown={blockInvalidNumberKeys}
                    />
                  </div>
                </div>
                <div className="flex flex-col space-y-1.5">
                  <Label>Front Left Shock Damping</Label>
                  <div className="grid grid-cols-2 gap-2">
                    <Input
                      id="flLSC"
                      placeholder="LSC"
                      type="number"
                      inputMode="numeric"
                      value={newEventState.flLSC ?? ""}
                      onChange={handleInputChange}
                      onKeyDown={blockInvalidNumberKeys}
                    />
                    <Input
                      id="flLSR"
                      placeholder="LSR"
                      type="number"
                      inputMode="numeric"
                      value={newEventState.flLSR ?? ""}
                      onChange={handleInputChange}
                      onKeyDown={blockInvalidNumberKeys}
                    />
                    <Input
                      id="flHSC"
                      placeholder="HSC"
                      type="number"
                      inputMode="numeric"
                      value={newEventState.flHSC ?? ""}
                      onChange={handleInputChange}
                      onKeyDown={blockInvalidNumberKeys}
                    />
                    <Input
                      id="flHSR"
                      placeholder="HSR"
                      type="number"
                      inputMode="numeric"
                      value={newEventState.flHSR ?? ""}
                      onChange={handleInputChange}
                      onKeyDown={blockInvalidNumberKeys}
                    />
                  </div>
                </div>
                <div className="flex flex-col space-y-1.5">
                  <Label>Rear Right Shock Damping</Label>
                  <div className="grid grid-cols-2 gap-2">
                    <Input
                      id="rrLSC"
                      placeholder="LSC"
                      type="number"
                      inputMode="numeric"
                      value={newEventState.rrLSC ?? ""}
                      onChange={handleInputChange}
                      onKeyDown={blockInvalidNumberKeys}
                    />
                    <Input
                      id="rrLSR"
                      placeholder="LSR"
                      type="number"
                      inputMode="numeric"
                      value={newEventState.rrLSR ?? ""}
                      onChange={handleInputChange}
                      onKeyDown={blockInvalidNumberKeys}
                    />
                    <Input
                      id="rrHSC"
                      placeholder="HSC"
                      type="number"
                      inputMode="numeric"
                      value={newEventState.rrHSC ?? ""}
                      onChange={handleInputChange}
                      onKeyDown={blockInvalidNumberKeys}
                    />
                    <Input
                      id="rrHSR"
                      placeholder="HSR"
                      type="number"
                      inputMode="numeric"
                      value={newEventState.rrHSR ?? ""}
                      onChange={handleInputChange}
                      onKeyDown={blockInvalidNumberKeys}
                    />
                  </div>
                </div>
                <div className="flex flex-col space-y-1.5">
                  <Label>Rear Left Shock Damping</Label>
                  <div className="grid grid-cols-2 gap-2">
                    <Input
                      id="rlLSC"
                      placeholder="LSC"
                      type="number"
                      inputMode="numeric"
                      value={newEventState.rlLSC ?? ""}
                      onChange={handleInputChange}
                      onKeyDown={blockInvalidNumberKeys}
                    />
                    <Input
                      id="rlLSR"
                      placeholder="LSR"
                      type="number"
                      inputMode="numeric"
                      value={newEventState.rlLSR ?? ""}
                      onChange={handleInputChange}
                      onKeyDown={blockInvalidNumberKeys}
                    />
                    <Input
                      id="rlHSC"
                      placeholder="HSC"
                      type="number"
                      inputMode="numeric"
                      value={newEventState.rlHSC ?? ""}
                      onChange={handleInputChange}
                      onKeyDown={blockInvalidNumberKeys}
                    />
                    <Input
                      id="rlHSR"
                      placeholder="HSR"
                      type="number"
                      inputMode="numeric"
                      value={newEventState.rlHSR ?? ""}
                      onChange={handleInputChange}
                      onKeyDown={blockInvalidNumberKeys}
                    />
                  </div>
                </div>
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="torqueLimit">Torque Limit</Label>
                <Input
                  id="torqueLimit"
                  type="number"
                  inputMode="decimal"
                  value={newEventState.torqueLimit ?? ""}
                  onChange={handleInputChange}
                  onKeyDown={blockInvalidNumberKeys}
                />
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="frPressure">FR Cold Pressure (psi)</Label>
                <Input
                  id="frPressure"
                  type="number"
                  inputMode="decimal"
                  value={newEventState.frPressure ?? ""}
                  onChange={handleInputChange}
                  onKeyDown={blockInvalidNumberKeys}
                />
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="flPressure">FL Cold Pressure (psi)</Label>
                <Input
                  id="flPressure"
                  type="number"
                  inputMode="decimal"
                  value={newEventState.flPressure ?? ""}
                  onChange={handleInputChange}
                  onKeyDown={blockInvalidNumberKeys}
                />
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="rrPressure">RR Cold Pressure (psi)</Label>
                <Input
                  id="rrPressure"
                  type="number"
                  inputMode="decimal"
                  value={newEventState.rrPressure ?? ""}
                  onChange={handleInputChange}
                  onKeyDown={blockInvalidNumberKeys}
                />
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="rlPressure">RL Cold Pressure (psi)</Label>
                <Input
                  id="rlPressure"
                  type="number"
                  inputMode="decimal"
                  value={newEventState.rlPressure ?? ""}
                  onChange={handleInputChange}
                  onKeyDown={blockInvalidNumberKeys}
                />
              </div>
              <div className="col-span-full grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
                <div className="flex flex-col space-y-1.5">
                  <Label htmlFor="frWearDepth">FR Tire Wear Depth</Label>
                  <Input
                    id="frWearDepth"
                    type="number"
                    inputMode="decimal"
                    value={newEventState.frWearDepth ?? ""}
                    onChange={handleInputChange}
                    onKeyDown={blockInvalidNumberKeys}
                  />
                </div>
                <div className="flex flex-col space-y-1.5">
                  <Label htmlFor="flWearDepth">FL Tire Wear Depth</Label>
                  <Input
                    id="flWearDepth"
                    type="number"
                    inputMode="decimal"
                    value={newEventState.flWearDepth ?? ""}
                    onChange={handleInputChange}
                    onKeyDown={blockInvalidNumberKeys}
                  />
                </div>
                <div className="flex flex-col space-y-1.5">
                  <Label htmlFor="rrWearDepth">RR Tire Wear Depth</Label>
                  <Input
                    id="rrWearDepth"
                    type="number"
                    inputMode="decimal"
                    value={newEventState.rrWearDepth ?? ""}
                    onChange={handleInputChange}
                    onKeyDown={blockInvalidNumberKeys}
                  />
                </div>
                <div className="flex flex-col space-y-1.5">
                  <Label htmlFor="rlWearDepth">RL Tire Wear Depth</Label>
                  <Input
                    id="rlWearDepth"
                    type="number"
                    inputMode="decimal"
                    value={newEventState.rlWearDepth ?? ""}
                    onChange={handleInputChange}
                    onKeyDown={blockInvalidNumberKeys}
                  />
                </div>
                <div className="flex flex-col space-y-1.5">
                  <Label htmlFor="frDurometer">FR Tire Durometer</Label>
                  <Input
                    id="frDurometer"
                    type="number"
                    inputMode="decimal"
                    value={newEventState.frDurometer ?? ""}
                    onChange={handleInputChange}
                    onKeyDown={blockInvalidNumberKeys}
                  />
                </div>
                <div className="flex flex-col space-y-1.5">
                  <Label htmlFor="flDurometer">FL Tire Durometer</Label>
                  <Input
                    id="flDurometer"
                    type="number"
                    inputMode="decimal"
                    value={newEventState.flDurometer ?? ""}
                    onChange={handleInputChange}
                    onKeyDown={blockInvalidNumberKeys}
                  />
                </div>
                <div className="flex flex-col space-y-1.5">
                  <Label htmlFor="rrDurometer">RR Tire Durometer</Label>
                  <Input
                    id="rrDurometer"
                    type="number"
                    inputMode="decimal"
                    value={newEventState.rrDurometer ?? ""}
                    onChange={handleInputChange}
                    onKeyDown={blockInvalidNumberKeys}
                  />
                </div>
                <div className="flex flex-col space-y-1.5">
                  <Label htmlFor="rlDurometer">RL Tire Durometer</Label>
                  <Input
                    id="rlDurometer"
                    type="number"
                    inputMode="decimal"
                    value={newEventState.rlDurometer ?? ""}
                    onChange={handleInputChange}
                    onKeyDown={blockInvalidNumberKeys}
                  />
                </div>
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="frontWingPitch">Front Wing Pitch</Label>
                <Input
                  id="frontWingPitch"
                  type="number"
                  inputMode="decimal"
                  value={newEventState.frontWingPitch ?? ""}
                  onChange={handleInputChange}
                  onKeyDown={blockInvalidNumberKeys}
                />
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="rearWingPitch">Rear Wing Pitch</Label>
                <Input
                  id="rearWingPitch"
                  type="number"
                  inputMode="decimal"
                  value={newEventState.rearWingPitch ?? ""}
                  onChange={handleInputChange}
                  onKeyDown={blockInvalidNumberKeys}
                />
              </div>
              {(() => {
                // Gate specialty fields based on selected car (carId=2 treated as Angelique)
                const isAngelique = newEventState.carId === 3;
                if (isAngelique) {
                  // Angelique-only: roll/heave springs (front/rear)
                  return (
                    <div className="col-span-full grid grid-cols-1 md:grid-cols-2 gap-4">
                      <div className="flex flex-col space-y-1.5">
                        <Label htmlFor="frontRollSpringRate">
                          Front Roll Spring Rate
                        </Label>
                        <Input
                          id="frontRollSpringRate"
                          type="number"
                          inputMode="decimal"
                          value={newEventState.frontRollSpringRate ?? ""}
                          onChange={handleInputChange}
                          onKeyDown={blockInvalidNumberKeys}
                        />
                      </div>
                      <div className="flex flex-col space-y-1.5">
                        <Label htmlFor="frontHeaveSpringRate">
                          Front Heave Spring Rate
                        </Label>
                        <Input
                          id="frontHeaveSpringRate"
                          type="number"
                          inputMode="decimal"
                          value={newEventState.frontHeaveSpringRate ?? ""}
                          onChange={handleInputChange}
                          onKeyDown={blockInvalidNumberKeys}
                        />
                      </div>
                      <div className="flex flex-col space-y-1.5">
                        <Label htmlFor="rearRollSpringRate">
                          Rear Roll Spring Rate
                        </Label>
                        <Input
                          id="rearRollSpringRate"
                          type="number"
                          inputMode="decimal"
                          value={newEventState.rearRollSpringRate ?? ""}
                          onChange={handleInputChange}
                          onKeyDown={blockInvalidNumberKeys}
                        />
                      </div>
                      <div className="flex flex-col space-y-1.5">
                        <Label htmlFor="rearHeaveSpringRate">
                          Rear Heave Spring Rate
                        </Label>
                        <Input
                          id="rearHeaveSpringRate"
                          type="number"
                          inputMode="decimal"
                          value={newEventState.rearHeaveSpringRate ?? ""}
                          onChange={handleInputChange}
                          onKeyDown={blockInvalidNumberKeys}
                        />
                      </div>
                    </div>
                  );
                }
                // Any other car (2026 car): corner springs + ARB settings
                return (
                  <>
                    <div className="col-span-full grid grid-cols-1 md:grid-cols-2 gap-4">
                      <div className="flex flex-col space-y-1.5">
                        <Label htmlFor="frontCornerSpringRate">
                          Front Corner Spring Rate
                        </Label>
                        <Input
                          id="frontCornerSpringRate"
                          type="number"
                          inputMode="decimal"
                          value={newEventState.frontCornerSpringRate ?? ""}
                          onChange={handleInputChange}
                          onKeyDown={blockInvalidNumberKeys}
                        />
                      </div>
                      <div className="flex flex-col space-y-1.5">
                        <Label htmlFor="rearCornerSpringRate">
                          Rear Corner Spring Rate
                        </Label>
                        <Input
                          id="rearCornerSpringRate"
                          type="number"
                          inputMode="decimal"
                          value={newEventState.rearCornerSpringRate ?? ""}
                          onChange={handleInputChange}
                          onKeyDown={blockInvalidNumberKeys}
                        />
                      </div>
                    </div>
                    <div className="col-span-full grid grid-cols-1 md:grid-cols-2 gap-4 mt-2">
                      <div className="flex flex-col space-y-1.5">
                        <Label htmlFor="frontArbSetting">
                          Front ARB Setting
                        </Label>
                        <Select
                          onValueChange={(v) =>
                            handleSelectChange("frontArbSetting", v)
                          }
                          value={newEventState.frontArbSetting}
                        >
                          <SelectTrigger>
                            <SelectValue placeholder="Select ARB setting" />
                          </SelectTrigger>
                          <SelectContent>
                            <SelectItem value="low">Low</SelectItem>
                            <SelectItem value="medium">Medium</SelectItem>
                            <SelectItem value="stiff">Stiff</SelectItem>
                          </SelectContent>
                        </Select>
                      </div>
                      <div className="flex flex-col space-y-1.5">
                        <Label htmlFor="rearArbSetting">Rear ARB Setting</Label>
                        <Select
                          onValueChange={(v) =>
                            handleSelectChange("rearArbSetting", v)
                          }
                          value={newEventState.rearArbSetting}
                        >
                          <SelectTrigger>
                            <SelectValue placeholder="Select ARB setting" />
                          </SelectTrigger>
                          <SelectContent>
                            <SelectItem value="low">Low</SelectItem>
                            <SelectItem value="medium">Medium</SelectItem>
                            <SelectItem value="stiff">Stiff</SelectItem>
                          </SelectContent>
                        </Select>
                      </div>
                    </div>
                  </>
                );
              })()}
            </div>
            <div className="mt-6 flex space-x-4">
              <div className="flex items-center space-x-2">
                <Checkbox
                  id="frontWingOn"
                  checked={newEventState.frontWingOn}
                  onCheckedChange={(c) =>
                    handleCheckboxChange("frontWingOn", c as boolean)
                  }
                />
                <Label htmlFor="frontWingOn">Front Wing On</Label>
              </div>
              <div className="flex items-center space-x-2">
                <Checkbox
                  id="rearWingOn"
                  checked={newEventState.rearWingOn}
                  onCheckedChange={(c) =>
                    handleCheckboxChange("rearWingOn", c as boolean)
                  }
                />
                <Label htmlFor="rearWingOn">Rear Wing On</Label>
              </div>
              <div className="flex items-center space-x-2">
                <Checkbox
                  id="regenOn"
                  checked={newEventState.regenOn}
                  onCheckedChange={(c) =>
                    handleCheckboxChange("regenOn", c as boolean)
                  }
                />
                <Label htmlFor="regenOn">Regen On</Label>
              </div>
              <div className="flex items-center space-x-2">
                <Checkbox
                  id="undertrayOn"
                  checked={newEventState.undertrayOn}
                  onCheckedChange={(c) =>
                    handleCheckboxChange("undertrayOn", c as boolean)
                  }
                />
                <Label htmlFor="undertrayOn">Undertray On</Label>
              </div>
            </div>
            <div className="mt-6 flex justify-end">
              <Button type="button" onClick={handleCreateEvent}>
                Create Event
              </Button>
            </div>
          </form>
        </CardContent>
      </Card>
    </div>
  );
}
