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
import { AppState, DriveDayState } from "@/lib/types";

const today = new Date().toISOString().slice(0, 10);

export default function DrivedayPage() {
  const router = useRouter();
  const [clientId] = useState(() => crypto.randomUUID());
  const [appState, setAppState] = useState<AppState>({});
  const [driveDayState, setDriveDayState] = useState<DriveDayState>({});
  const [submitError, setSubmitError] = useState<string>("");
  const [date, setDate] = useState<string>(today);

  // Buffers for in-progress decimal input (e.g. "12." or ".5")
  const [floatInput, setFloatInput] = useState<Record<string, string>>({});

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
      }
    };
    return () => eventSource.close();
  }, [clientId]);

  const numericStringAllowed = (s: string) => /^-?\d*(?:\.\d*)?$/.test(s);
  const integerStringAllowed = (s: string) => /^\d*$/.test(s);

  const blockInvalidNumberKeys = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (["e", "E", "+"].includes(e.key)) e.preventDefault();
  };

  // Float fields stored as numbers on DriveDayState
  const floatFields: Array<keyof DriveDayState> = [
    "airTemperature",
    "relativeHumidity",
    "trackTemperature",
  ];

  // Numeric string fields (stored as strings, validated as numeric)
  const numericStringFields: Array<keyof DriveDayState> = [
    "windSpeed",
    "carWeight", "towAngle",
    "camberFront", "camberRear", "toeFront", "toeRear",
    "rideHeightFront", "rideHeightRear",
    "frLSC", "frLSR", "frHSC", "frHSR",
    "flLSC", "flLSR", "flHSC", "flHSR",
    "rrLSC", "rrLSR", "rrHSC", "rrHSR",
    "rlLSC", "rlLSR", "rlHSC", "rlHSR",
    "torqueLimit",
    "frPressure", "flPressure", "rrPressure", "rlPressure",
    "frHotPressure", "flHotPressure", "rrHotPressure", "rlHotPressure",
    "frWearDepth", "flWearDepth", "rrWearDepth", "rlWearDepth",
    "frDurometer", "flDurometer", "rrDurometer", "rlDurometer",
    "frontWingPitch", "rearWingPitch",
    "frontRollSpringRate", "frontHeaveSpringRate",
    "rearRollSpringRate", "rearHeaveSpringRate",
    "frontCornerSpringRate", "rearCornerSpringRate",
  ];

  const handleInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const { id } = e.target;
    const valueStr = e.target.value;
    const updated: DriveDayState = { ...driveDayState };

    if (floatFields.includes(id as keyof DriveDayState)) {
      if (valueStr === "") {
        (updated as any)[id] = undefined;
        setFloatInput((prev) => ({ ...prev, [id]: "" }));
      } else if (numericStringAllowed(valueStr)) {
        setFloatInput((prev) => ({ ...prev, [id]: valueStr }));
        if (/[0-9]/.test(valueStr)) {
          const n = parseFloat(valueStr);
          if (Number.isFinite(n)) (updated as any)[id] = n;
        }
      } else {
        return;
      }
    } else if (numericStringFields.includes(id as keyof DriveDayState)) {
      if (valueStr === "" || numericStringAllowed(valueStr)) {
        (updated as any)[id] = valueStr === "" ? undefined : valueStr;
      } else {
        return;
      }
    } else {
      (updated as any)[id] = valueStr === "" ? undefined : valueStr;
    }

    setDriveDayState(updated);
    sendStateUpdate({ driveDay: updated });
  };

  const handleSelectChange = (id: keyof DriveDayState, value: string) => {
    const numericIdFields: Array<keyof DriveDayState> = ["driverId", "carId", "eventType"];
    let parsedValue: any = value;
    if (numericIdFields.includes(id)) {
      parsedValue = parseInt(value, 10);
    }
    const updated = { ...driveDayState, [id]: parsedValue };
    setDriveDayState(updated);
    sendStateUpdate({ driveDay: updated });
  };

  const handleCheckboxChange = (id: keyof DriveDayState, checked: boolean) => {
    const updated = { ...driveDayState, [id]: checked };
    setDriveDayState(updated);
    sendStateUpdate({ driveDay: updated });
  };

  const handleCreate = async () => {
    setSubmitError("");
    const response = await fetch("/api/new-drive-day", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        date,
        track_name: driveDayState.trackName,
        weather: driveDayState.weather,
        wind_speed: driveDayState.windSpeed,
        air_temperature: driveDayState.airTemperature,
        relative_humidity: driveDayState.relativeHumidity,
        track_temperature: driveDayState.trackTemperature,
        car_id: driveDayState.carId,
        driver_id: driveDayState.driverId,
        event_type: driveDayState.eventType,
        location_id: 0,
        car_weight: driveDayState.carWeight,
        tow_angle: driveDayState.towAngle,
        camber_front: driveDayState.camberFront,
        camber_rear: driveDayState.camberRear,
        toe_front: driveDayState.toeFront,
        toe_rear: driveDayState.toeRear,
        ride_height_front: driveDayState.rideHeightFront,
        ride_height_rear: driveDayState.rideHeightRear,
        torque_limit: driveDayState.torqueLimit,
        fr_lsc: driveDayState.frLSC, fr_lsr: driveDayState.frLSR,
        fr_hsc: driveDayState.frHSC, fr_hsr: driveDayState.frHSR,
        fl_lsc: driveDayState.flLSC, fl_lsr: driveDayState.flLSR,
        fl_hsc: driveDayState.flHSC, fl_hsr: driveDayState.flHSR,
        rr_lsc: driveDayState.rrLSC, rr_lsr: driveDayState.rrLSR,
        rr_hsc: driveDayState.rrHSC, rr_hsr: driveDayState.rrHSR,
        rl_lsc: driveDayState.rlLSC, rl_lsr: driveDayState.rlLSR,
        rl_hsc: driveDayState.rlHSC, rl_hsr: driveDayState.rlHSR,
        frw_pressure: driveDayState.frPressure,
        flw_pressure: driveDayState.flPressure,
        brw_pressure: driveDayState.rrPressure,
        blw_pressure: driveDayState.rlPressure,
        frw_hot_pressure: driveDayState.frHotPressure,
        flw_hot_pressure: driveDayState.flHotPressure,
        brw_hot_pressure: driveDayState.rrHotPressure,
        blw_hot_pressure: driveDayState.rlHotPressure,
        fr_wear_depth: driveDayState.frWearDepth,
        fl_wear_depth: driveDayState.flWearDepth,
        rr_wear_depth: driveDayState.rrWearDepth,
        rl_wear_depth: driveDayState.rlWearDepth,
        fr_durometer: driveDayState.frDurometer,
        fl_durometer: driveDayState.flDurometer,
        rr_durometer: driveDayState.rrDurometer,
        rl_durometer: driveDayState.rlDurometer,
        front_wing_on: driveDayState.frontWingOn,
        rear_wing_on: driveDayState.rearWingOn,
        front_wing_pitch: driveDayState.frontWingPitch,
        rear_wing_pitch: driveDayState.rearWingPitch,
        regen_on: driveDayState.regenOn,
        undertray_on: driveDayState.undertrayOn,
        front_roll_spring_rate: driveDayState.frontRollSpringRate,
        front_heave_spring_rate: driveDayState.frontHeaveSpringRate,
        rear_roll_spring_rate: driveDayState.rearRollSpringRate,
        rear_heave_spring_rate: driveDayState.rearHeaveSpringRate,
        front_corner_spring_rate: driveDayState.frontCornerSpringRate,
        rear_corner_spring_rate: driveDayState.rearCornerSpringRate,
        front_arb_setting: driveDayState.frontArbSetting,
        rear_arb_setting: driveDayState.rearArbSetting,
      }),
    });

    if (response.status !== 201) {
      setSubmitError("Failed to create drive day. Please try again.");
      return;
    }

    const { day_id } = await response.json();
    const updatedState = { ...driveDayState, dayId: day_id };
    sendStateUpdate({ driveDay: updatedState, currentPage: "/driveday-active" });
    router.push("/live-viewer");
  };

  const floatVal = (id: string, field: keyof DriveDayState) =>
    floatInput[id] ?? (driveDayState[field] != null ? String(driveDayState[field]) : "");

  const SectionHeader = ({ title }: { title: string }) => (
    <h3 className="col-span-full text-xs font-semibold uppercase tracking-widest text-muted-foreground border-b pb-2 mb-1">
      {title}
    </h3>
  );

  return (
    <div className="p-8 flex justify-center items-center pt-20">
      <Card className="w-full max-w-4xl">
        <CardHeader>
          <CardTitle>New Drive Day</CardTitle>
          <CardDescription>
            Enter drive day conditions, car setup, and tire data.
          </CardDescription>
        </CardHeader>
        <CardContent>
          <form>
            <div className="flex flex-col gap-10">

              {/* ── Session ── */}
              <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-x-6 gap-y-4">
                <SectionHeader title="Session" />
                <div className="flex flex-col space-y-1.5">
                  <Label htmlFor="date">Date</Label>
                  <Input id="date" type="date" value={date} onChange={(e) => setDate(e.target.value)} />
                </div>
                <div className="flex flex-col space-y-1.5">
                  <Label htmlFor="trackName">Track Name</Label>
                  <Input id="trackName" type="text" placeholder="Enter track name" value={driveDayState.trackName ?? ""} onChange={handleInputChange} />
                </div>
                <div className="flex flex-col space-y-1.5">
                  <Label>Event Type</Label>
                  <Select onValueChange={(v) => handleSelectChange("eventType", v)} value={driveDayState.eventType?.toString() ?? ""}>
                    <SelectTrigger><SelectValue placeholder="Select event type" /></SelectTrigger>
                    <SelectContent>
                      <SelectItem value="0">Other</SelectItem>
                      <SelectItem value="1">Endurance</SelectItem>
                      <SelectItem value="2">Autocross</SelectItem>
                      <SelectItem value="3">Skidpad</SelectItem>
                      <SelectItem value="4">Straight Line Acceleration</SelectItem>
                      <SelectItem value="5">Straight Line Braking</SelectItem>
                    </SelectContent>
                  </Select>
                </div>
                <div className="flex flex-col space-y-1.5">
                  <Label>Driver</Label>
                  <Select onValueChange={(v) => handleSelectChange("driverId", v)} value={driveDayState.driverId?.toString() ?? ""}>
                    <SelectTrigger><SelectValue placeholder="Select driver" /></SelectTrigger>
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
                  <Label>Car</Label>
                  <Select onValueChange={(v) => handleSelectChange("carId", v)} value={driveDayState.carId?.toString() ?? ""}>
                    <SelectTrigger><SelectValue placeholder="Select car" /></SelectTrigger>
                    <SelectContent>
                      <SelectItem value="1">Easy Driver</SelectItem>
                      <SelectItem value="2">Lady Luck</SelectItem>
                      <SelectItem value="3">Angelique</SelectItem>
                      <SelectItem value="4">Nightwatch</SelectItem>
                      <SelectItem value="5">Orion</SelectItem>
                    </SelectContent>
                  </Select>
                </div>
              </div>

              {/* ── Conditions ── */}
              <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-x-6 gap-y-4">
                <SectionHeader title="Conditions" />
                <div className="flex flex-col space-y-1.5">
                  <Label htmlFor="weather">Weather</Label>
                  <Input id="weather" type="text" placeholder="e.g. Sunny, cloudy" value={driveDayState.weather ?? ""} onChange={handleInputChange} />
                </div>
                <div className="flex flex-col space-y-1.5">
                  <Label htmlFor="windSpeed">Wind Speed (mph)</Label>
                  <Input id="windSpeed" type="text" inputMode="decimal" placeholder="mph" value={driveDayState.windSpeed ?? ""} onChange={handleInputChange} onKeyDown={blockInvalidNumberKeys} />
                </div>
                <div className="flex flex-col space-y-1.5">
                  <Label htmlFor="relativeHumidity">Humidity (%)</Label>
                  <Input id="relativeHumidity" type="text" inputMode="decimal" placeholder="%" value={floatVal("relativeHumidity", "relativeHumidity")} onChange={handleInputChange} />
                </div>
                <div className="flex flex-col space-y-1.5">
                  <Label htmlFor="trackTemperature">Track Temp (°F)</Label>
                  <Input id="trackTemperature" type="text" inputMode="decimal" placeholder="°F" value={floatVal("trackTemperature", "trackTemperature")} onChange={handleInputChange} />
                </div>
                <div className="flex flex-col space-y-1.5">
                  <Label htmlFor="airTemperature">Ambient Temp (°F)</Label>
                  <Input id="airTemperature" type="text" inputMode="decimal" placeholder="°F" value={floatVal("airTemperature", "airTemperature")} onChange={handleInputChange} />
                </div>
              </div>

              {/* ── Car Setup ── */}
              <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-x-6 gap-y-4">
                <SectionHeader title="Car Setup" />
                {([
                  { id: "carWeight",       label: "Car Weight (lbs)",    mode: "decimal" },
                  { id: "torqueLimit",     label: "Torque Limit (Nm)",   mode: "numeric" },
                  { id: "towAngle",        label: "Tow Angle (°)",       mode: "decimal" },
                  { id: "camberFront",     label: "Camber Front (°)",    mode: "decimal" },
                  { id: "camberRear",      label: "Camber Rear (°)",     mode: "decimal" },
                  { id: "toeFront",        label: "Toe Front (°)",       mode: "decimal" },
                  { id: "toeRear",         label: "Toe Rear (°)",        mode: "decimal" },
                  { id: "rideHeightFront", label: "Ride Height Front",   mode: "decimal" },
                  { id: "rideHeightRear",  label: "Ride Height Rear",    mode: "decimal" },
                ] as const).map(({ id, label, mode }) => (
                  <div key={id} className="flex flex-col space-y-1.5">
                    <Label htmlFor={id}>{label}</Label>
                    <Input id={id} type="text" inputMode={mode} value={(driveDayState as any)[id] ?? ""} onChange={handleInputChange} onKeyDown={blockInvalidNumberKeys} />
                  </div>
                ))}
              </div>

              {/* ── Shock Damping ── */}
              <div className="grid grid-cols-1 gap-y-4">
                <SectionHeader title="Shock Damping" />
                <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
                  {(
                    [
                      { label: "Front Right", lsc: "frLSC", lsr: "frLSR", hsc: "frHSC", hsr: "frHSR" },
                      { label: "Front Left",  lsc: "flLSC", lsr: "flLSR", hsc: "flHSC", hsr: "flHSR" },
                      { label: "Rear Right",  lsc: "rrLSC", lsr: "rrLSR", hsc: "rrHSC", hsr: "rrHSR" },
                      { label: "Rear Left",   lsc: "rlLSC", lsr: "rlLSR", hsc: "rlHSC", hsr: "rlHSR" },
                    ] as const
                  ).map(({ label, lsc, lsr, hsc, hsr }) => (
                    <div key={lsc} className="flex flex-col space-y-2">
                      <p className="text-xs font-medium text-muted-foreground">{label}</p>
                      <div className="grid grid-cols-2 gap-2">
                        {([{ id: lsc, ph: "LSC" }, { id: lsr, ph: "LSR" }, { id: hsc, ph: "HSC" }, { id: hsr, ph: "HSR" }] as const).map(({ id, ph }) => (
                          <Input key={id} id={id} placeholder={ph} type="text" inputMode="numeric" value={(driveDayState as any)[id] ?? ""} onChange={handleInputChange} onKeyDown={blockInvalidNumberKeys} />
                        ))}
                      </div>
                    </div>
                  ))}
                </div>
              </div>

              {/* ── Tires ── */}
              <div className="grid grid-cols-1 gap-y-4">
                <SectionHeader title="Tires" />
                <div className="grid grid-cols-1 gap-y-5">
                  {([
                    { sublabel: "Cold Pressures (psi)", fields: [
                      { id: "frPressure", label: "FR" }, { id: "flPressure", label: "FL" },
                      { id: "rrPressure", label: "RR" }, { id: "rlPressure", label: "RL" },
                    ]},
                    { sublabel: "Hot Pressures (psi)", fields: [
                      { id: "frHotPressure", label: "FR" }, { id: "flHotPressure", label: "FL" },
                      { id: "rrHotPressure", label: "RR" }, { id: "rlHotPressure", label: "RL" },
                    ]},
                    { sublabel: "Wear Depth", fields: [
                      { id: "frWearDepth", label: "FR" }, { id: "flWearDepth", label: "FL" },
                      { id: "rrWearDepth", label: "RR" }, { id: "rlWearDepth", label: "RL" },
                    ]},
                    { sublabel: "Durometer", fields: [
                      { id: "frDurometer", label: "FR" }, { id: "flDurometer", label: "FL" },
                      { id: "rrDurometer", label: "RR" }, { id: "rlDurometer", label: "RL" },
                    ]},
                  ] as const).map(({ sublabel, fields }) => (
                    <div key={sublabel}>
                      <p className="text-xs font-medium text-muted-foreground mb-2">{sublabel}</p>
                      <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
                        {fields.map(({ id, label }) => (
                          <div key={id} className="flex flex-col space-y-1.5">
                            <Label htmlFor={id}>{label}</Label>
                            <Input id={id} type="text" inputMode="decimal" value={(driveDayState as any)[id] ?? ""} onChange={handleInputChange} onKeyDown={blockInvalidNumberKeys} />
                          </div>
                        ))}
                      </div>
                    </div>
                  ))}
                </div>
              </div>

              {/* ── Aero & Springs ── */}
              <div className="grid grid-cols-1 gap-y-4">
                <SectionHeader title="Aero & Springs" />
                <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-x-6 gap-y-4">
                  <div className="flex flex-col space-y-1.5">
                    <Label htmlFor="frontWingPitch">Front Wing Pitch</Label>
                    <Input id="frontWingPitch" type="text" inputMode="decimal" value={driveDayState.frontWingPitch ?? ""} onChange={handleInputChange} onKeyDown={blockInvalidNumberKeys} />
                  </div>
                  <div className="flex flex-col space-y-1.5">
                    <Label htmlFor="rearWingPitch">Rear Wing Pitch</Label>
                    <Input id="rearWingPitch" type="text" inputMode="decimal" value={driveDayState.rearWingPitch ?? ""} onChange={handleInputChange} onKeyDown={blockInvalidNumberKeys} />
                  </div>
                  <div className="flex flex-col space-y-1.5">
                    <Label htmlFor="frontCornerSpringRate">Front Corner Spring</Label>
                    <Input id="frontCornerSpringRate" type="text" inputMode="decimal" value={driveDayState.frontCornerSpringRate ?? ""} onChange={handleInputChange} onKeyDown={blockInvalidNumberKeys} />
                  </div>
                  <div className="flex flex-col space-y-1.5">
                    <Label htmlFor="rearCornerSpringRate">Rear Corner Spring</Label>
                    <Input id="rearCornerSpringRate" type="text" inputMode="decimal" value={driveDayState.rearCornerSpringRate ?? ""} onChange={handleInputChange} onKeyDown={blockInvalidNumberKeys} />
                  </div>
                  <div className="flex flex-col space-y-1.5">
                    <Label>Front ARB Setting</Label>
                    <Select onValueChange={(v) => handleSelectChange("frontArbSetting", v)} value={driveDayState.frontArbSetting ?? ""}>
                      <SelectTrigger><SelectValue placeholder="Select" /></SelectTrigger>
                      <SelectContent>
                        <SelectItem value="low">Low</SelectItem>
                        <SelectItem value="medium">Medium</SelectItem>
                        <SelectItem value="stiff">Stiff</SelectItem>
                      </SelectContent>
                    </Select>
                  </div>
                  <div className="flex flex-col space-y-1.5">
                    <Label>Rear ARB Setting</Label>
                    <Select onValueChange={(v) => handleSelectChange("rearArbSetting", v)} value={driveDayState.rearArbSetting ?? ""}>
                      <SelectTrigger><SelectValue placeholder="Select" /></SelectTrigger>
                      <SelectContent>
                        <SelectItem value="low">Low</SelectItem>
                        <SelectItem value="medium">Medium</SelectItem>
                        <SelectItem value="stiff">Stiff</SelectItem>
                      </SelectContent>
                    </Select>
                  </div>
                  <div className="col-span-full flex flex-wrap gap-6 pt-2">
                    {(
                      [
                        { id: "frontWingOn", label: "Front Wing On" },
                        { id: "rearWingOn",  label: "Rear Wing On"  },
                        { id: "regenOn",     label: "Regen On"      },
                        { id: "undertrayOn", label: "Undertray On"  },
                      ] as const
                    ).map(({ id, label }) => (
                      <div key={id} className="flex items-center space-x-2">
                        <Checkbox id={id} checked={driveDayState[id] ?? false} onCheckedChange={(c) => handleCheckboxChange(id, c as boolean)} />
                        <Label htmlFor={id}>{label}</Label>
                      </div>
                    ))}
                  </div>
                </div>
              </div>

            </div>

            {submitError && (
              <p className="mt-6 text-sm text-red-600">{submitError}</p>
            )}

            <div className="mt-8 flex justify-end">
              <Button type="button" onClick={handleCreate}>Start Drive Day</Button>
            </div>
          </form>
        </CardContent>
      </Card>
    </div>
  );
}
