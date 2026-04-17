"use client";

import dynamic from "next/dynamic";
import { CSSProperties, useEffect, useMemo, useState } from "react";

import { useKafkaJSON } from "@/hooks/useKafkaStream";
import { useCarSelection } from "@/lib/carSelection";
import { LiveCar, SUPPORTED_LIVE_CARS, liveCarLabel } from "@/lib/car";

const LiveMap = dynamic(() => import("@/components/Map"), { ssr: false });
const THEME_STORAGE_KEY = "realtimeShowcaseTheme";

type DashboardData = {
  packetId?: number | null;
  speed?: number | null;
  wheelSpeedAvg?: number | null;
  steerColAngle?: number | null;
  throttlePct?: number | null;
  brakePct?: number | null;
  batteryPct?: number | null;
  hvPackV?: number | null;
  hvCurrent?: number | null;
  lvV?: number | null;
  inverterTempC?: number | null;
  motorTempC?: number | null;
  ambientTempC?: number | null;
};

type LiveBannerData = {
  battery?: number | null;
  odometer?: number | null;
};

type EnergyBudgetData = {
  powerKw?: number | null;
  timeSinceOnS?: number | null;
  batteryPct?: number | null;
};

type MapData = {
  dynamics?: { gps?: number[] | null };
};

type SensorData = {
  dynamics?: Record<string, unknown>;
  pack?: Record<string, unknown>;
  thermal?: Record<string, unknown>;
};

function toFiniteNumber(value: unknown): number | undefined {
  const n = typeof value === "number" ? value : Number(value);
  return Number.isFinite(n) ? n : undefined;
}

function firstFinite(...values: Array<unknown>): number | undefined {
  for (const value of values) {
    const n = toFiniteNumber(value);
    if (n !== undefined) return n;
  }
  return undefined;
}

function findNumberByKeys(
  obj: Record<string, unknown> | undefined,
  keys: string[],
): number | undefined {
  if (!obj) return undefined;
  for (const key of keys) {
    const n = toFiniteNumber(obj[key]);
    if (n !== undefined) return n;
  }
  return undefined;
}

function findNumberArrayByKeys(
  obj: Record<string, unknown> | undefined,
  keys: string[],
): number[] | undefined {
  if (!obj) return undefined;
  for (const key of keys) {
    const raw = obj[key];
    if (!Array.isArray(raw)) continue;
    const values = raw
      .map((entry) => toFiniteNumber(entry))
      .filter((entry): entry is number => entry !== undefined);
    if (values.length) return values;
  }
  return undefined;
}

function clamp(value: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, value));
}

function formatNumber(value: number | undefined, digits = 1): string {
  return typeof value === "number" ? value.toFixed(digits) : "—";
}

function formatInt(value: number | undefined): string {
  return typeof value === "number" ? Math.round(value).toString() : "—";
}

function statusTone(value: number | undefined): string {
  if (typeof value !== "number") return "text-slate-300";
  if (value >= 60) return "text-emerald-400";
  if (value >= 30) return "text-amber-300";
  return "text-rose-400";
}

type ShowcaseTheme = {
  id: string;
  label: string;
  description: string;
  pageBackground: string;
  panelBackground: string;
  panelBorder: string;
  panelShadow: string;
  chipBackground: string;
  chipBorder: string;
  cardBackground: string;
  cardBorder: string;
  accent: string;
  accentSoft: string;
  throttleGradient: string;
  brakeGradient: string;
  potGradient: string;
};

const SHOWCASE_THEMES: ShowcaseTheme[] = [
  {
    id: "longhorn-classic",
    label: "Longhorn Classic",
    description: "Burnt orange spotlight with rich dark background.",
    pageBackground:
      "radial-gradient(circle at 20% 0%, #5A2A08 0%, #140A06 38%, #090909 100%)",
    panelBackground: "rgba(0, 0, 0, 0.35)",
    panelBorder: "rgba(191, 87, 0, 0.34)",
    panelShadow: "0 20px 80px rgba(191, 87, 0, 0.22)",
    chipBackground: "rgba(0, 0, 0, 0.28)",
    chipBorder: "rgba(255, 255, 255, 0.2)",
    cardBackground: "rgba(255, 255, 255, 0.06)",
    cardBorder: "rgba(255, 255, 255, 0.14)",
    accent: "#BF5700",
    accentSoft: "#FFB070",
    throttleGradient: "linear-gradient(90deg, #0d7044 0%, #34d399 100%)",
    brakeGradient: "linear-gradient(90deg, #7f1d1d 0%, #fb7185 100%)",
    potGradient: "linear-gradient(180deg, #FFB070 0%, #BF5700 100%)",
  },
  {
    id: "carbon-race",
    label: "Carbon Race",
    description: "Graphite cockpit look with copper highlights.",
    pageBackground:
      "radial-gradient(circle at 15% -10%, #2f2f30 0%, #171819 35%, #090a0b 100%)",
    panelBackground: "rgba(15, 17, 19, 0.58)",
    panelBorder: "rgba(199, 138, 43, 0.35)",
    panelShadow: "0 20px 60px rgba(0, 0, 0, 0.35)",
    chipBackground: "rgba(25, 27, 29, 0.85)",
    chipBorder: "rgba(208, 160, 88, 0.4)",
    cardBackground: "rgba(37, 39, 41, 0.72)",
    cardBorder: "rgba(208, 160, 88, 0.25)",
    accent: "#C98A2B",
    accentSoft: "#F8C57D",
    throttleGradient: "linear-gradient(90deg, #047857 0%, #10b981 100%)",
    brakeGradient: "linear-gradient(90deg, #881337 0%, #f43f5e 100%)",
    potGradient: "linear-gradient(180deg, #F8C57D 0%, #C98A2B 100%)",
  },
  {
    id: "sunset-copper",
    label: "Sunset Copper",
    description: "Warm unveiling stage palette with copper glow.",
    pageBackground:
      "radial-gradient(circle at 80% -20%, #8f3f14 0%, #4a1f17 40%, #130f13 100%)",
    panelBackground: "rgba(24, 11, 16, 0.6)",
    panelBorder: "rgba(255, 125, 74, 0.35)",
    panelShadow: "0 20px 70px rgba(255, 97, 42, 0.15)",
    chipBackground: "rgba(40, 17, 24, 0.72)",
    chipBorder: "rgba(255, 162, 121, 0.4)",
    cardBackground: "rgba(255, 255, 255, 0.08)",
    cardBorder: "rgba(255, 205, 170, 0.22)",
    accent: "#FF7D4A",
    accentSoft: "#FFC29B",
    throttleGradient: "linear-gradient(90deg, #166534 0%, #22c55e 100%)",
    brakeGradient: "linear-gradient(90deg, #9f1239 0%, #fb7185 100%)",
    potGradient: "linear-gradient(180deg, #FFC29B 0%, #FF7D4A 100%)",
  },
  {
    id: "longhorn-noir",
    label: "Longhorn Noir",
    description: "Near-black premium theme with focused orange accents.",
    pageBackground:
      "radial-gradient(circle at 50% -10%, #362018 0%, #121214 37%, #050506 100%)",
    panelBackground: "rgba(10, 10, 12, 0.72)",
    panelBorder: "rgba(176, 85, 22, 0.36)",
    panelShadow: "0 25px 70px rgba(0, 0, 0, 0.45)",
    chipBackground: "rgba(20, 20, 22, 0.85)",
    chipBorder: "rgba(176, 85, 22, 0.42)",
    cardBackground: "rgba(255, 255, 255, 0.04)",
    cardBorder: "rgba(255, 255, 255, 0.13)",
    accent: "#B05516",
    accentSoft: "#E8A56B",
    throttleGradient: "linear-gradient(90deg, #14532d 0%, #4ade80 100%)",
    brakeGradient: "linear-gradient(90deg, #7f1d1d 0%, #f87171 100%)",
    potGradient: "linear-gradient(180deg, #E8A56B 0%, #B05516 100%)",
  },
];

function BarMetric({
  label,
  value,
  max,
  fill,
}: {
  label: string;
  value: number | undefined;
  max: number;
  fill: string;
}) {
  const pct = typeof value === "number" ? clamp((value / max) * 100, 0, 100) : 0;
  return (
    <div className="space-y-2">
      <div className="flex items-center justify-between text-sm">
        <span className="text-white/75">{label}</span>
        <span className="font-semibold text-white">{formatNumber(value, 1)}%</span>
      </div>
      <div className="h-2 overflow-hidden rounded-full bg-white/15">
        <div className="h-full rounded-full" style={{ width: `${pct}%`, background: fill }} />
      </div>
    </div>
  );
}

function SuspensionPot({
  corner,
  value,
  theme,
}: {
  corner: "FL" | "FR" | "BL" | "BR";
  value: number | undefined;
  theme: ShowcaseTheme;
}) {
  const pct = typeof value === "number" ? clamp((value / 5) * 100, 0, 100) : 0;
  return (
    <div
      className="rounded-xl border p-3"
      style={{ background: theme.cardBackground, borderColor: theme.cardBorder }}
    >
      <div className="mb-2 flex items-center justify-between text-xs uppercase tracking-wide text-white/70">
        <span>{corner}</span>
        <span>{formatNumber(value, 2)} V</span>
      </div>
      <div className="relative h-24 overflow-hidden rounded-lg bg-white/10">
        <div
          className="absolute inset-x-0 bottom-0 transition-all duration-200"
          style={{ height: `${pct}%`, background: theme.potGradient }}
          aria-hidden
        />
      </div>
    </div>
  );
}

function heatColor(temp: number, min: number, max: number): string {
  const normalized = max > min ? clamp((temp - min) / (max - min), 0, 1) : 0.5;
  const hue = 220 - normalized * 220;
  return `hsl(${hue} 90% 52%)`;
}

function CellHeatmap({
  values,
  theme,
}: {
  values: number[] | undefined;
  theme: ShowcaseTheme;
}) {
  if (!values?.length) {
    return (
      <div
        className="rounded-xl border p-4 text-sm text-white/70"
        style={{ background: theme.cardBackground, borderColor: theme.cardBorder }}
      >
        Waiting for cell temperature array from live telemetry.
      </div>
    );
  }

  const cols = values.length === 90 ? 10 : Math.ceil(Math.sqrt(values.length));
  const minTemp = Math.min(...values);
  const maxTemp = Math.max(...values);
  const hottestIndex = values.reduce(
    (bestIndex, value, index) => (value > values[bestIndex] ? index : bestIndex),
    0,
  );

  return (
    <div className="space-y-3">
      <div className="grid grid-cols-2 gap-2 text-xs md:grid-cols-4">
        <div
          className="rounded-lg border p-2"
          style={{ background: theme.cardBackground, borderColor: theme.cardBorder }}
        >
          <div className="text-white/65">Cells</div>
          <div className="font-semibold text-white">{values.length}</div>
        </div>
        <div
          className="rounded-lg border p-2"
          style={{ background: theme.cardBackground, borderColor: theme.cardBorder }}
        >
          <div className="text-white/65">Min</div>
          <div className="font-semibold text-white">{minTemp.toFixed(1)}°C</div>
        </div>
        <div
          className="rounded-lg border p-2"
          style={{ background: theme.cardBackground, borderColor: theme.cardBorder }}
        >
          <div className="text-white/65">Max</div>
          <div className="font-semibold text-white">{maxTemp.toFixed(1)}°C</div>
        </div>
        <div
          className="rounded-lg border p-2"
          style={{ background: theme.cardBackground, borderColor: theme.cardBorder }}
        >
          <div className="text-white/65">Hottest Cell</div>
          <div className="font-semibold text-white">#{hottestIndex + 1}</div>
        </div>
      </div>

      <div
        className="grid gap-1 rounded-xl border p-3"
        style={{
          background: theme.cardBackground,
          borderColor: theme.cardBorder,
          gridTemplateColumns: `repeat(${cols}, minmax(0, 1fr))`,
        }}
      >
        {values.map((temp, index) => (
          <div
            key={`cell-${index}`}
            className="aspect-square rounded-sm border border-black/25"
            style={{ background: heatColor(temp, minTemp, maxTemp) }}
            title={`Cell ${index + 1}: ${temp.toFixed(2)}°C`}
          />
        ))}
      </div>

      <div className="flex items-center justify-between gap-2 text-[11px] text-white/70">
        <span>Cool ({minTemp.toFixed(1)}°C)</span>
        <div
          className="h-2 w-full max-w-[180px] rounded-full"
          style={{ background: "linear-gradient(90deg, hsl(220 90% 52%), hsl(0 90% 52%))" }}
        />
        <span>Hot ({maxTemp.toFixed(1)}°C)</span>
      </div>
    </div>
  );
}

export default function RealtimeShowcasePage() {
  const {
    selectedCar,
    selectedCarLabel,
    multiCarEnabled,
    setSelectedCar,
    ssePath,
    matchesSelectedCar,
  } = useCarSelection();
  const [themeId, setThemeId] = useState(SHOWCASE_THEMES[0].id);

  useEffect(() => {
    if (typeof window === "undefined") return;
    const stored = window.localStorage.getItem(THEME_STORAGE_KEY);
    const exists = SHOWCASE_THEMES.some((theme) => theme.id === stored);
    if (stored && exists) setThemeId(stored);
  }, []);

  useEffect(() => {
    if (typeof window === "undefined") return;
    window.localStorage.setItem(THEME_STORAGE_KEY, themeId);
  }, [themeId]);

  const streamOpts = useMemo(
    () => ({
      car: selectedCar,
      ssePath,
      filter: matchesSelectedCar,
      staleAfterMs: 2200,
      sampleMs: 100,
      merge: true,
    }),
    [matchesSelectedCar, selectedCar, ssePath],
  );

  const activeTheme = useMemo(
    () => SHOWCASE_THEMES.find((theme) => theme.id === themeId) ?? SHOWCASE_THEMES[0],
    [themeId],
  );

  const panelStyle = useMemo<CSSProperties>(
    () => ({
      background: activeTheme.panelBackground,
      borderColor: activeTheme.panelBorder,
      boxShadow: activeTheme.panelShadow,
    }),
    [activeTheme],
  );

  const cardStyle = useMemo<CSSProperties>(
    () => ({
      background: activeTheme.cardBackground,
      borderColor: activeTheme.cardBorder,
    }),
    [activeTheme],
  );

  const { data: dashboard, kafkaConnected, lastMessageAt } = useKafkaJSON<DashboardData>({
    topic: "dashboard_screen",
    ...streamOpts,
  });
  const { data: liveBanner } = useKafkaJSON<LiveBannerData>({
    topic: "live_banner",
    ...streamOpts,
  });
  const { data: mapData } = useKafkaJSON<MapData>({
    topic: "map",
    ...streamOpts,
  });
  const { data: energy } = useKafkaJSON<EnergyBudgetData>({
    topic: "energy_budget",
    ...streamOpts,
  });
  const { data: sensorData } = useKafkaJSON<SensorData>({
    topic: "sensor_data",
    ...streamOpts,
  });

  const batteryPct = firstFinite(dashboard?.batteryPct, liveBanner?.battery, energy?.batteryPct);
  const hvPackV = firstFinite(dashboard?.hvPackV);
  const hvCurrent = firstFinite(dashboard?.hvCurrent);
  const powerKw = firstFinite(energy?.powerKw);
  const lvV = firstFinite(dashboard?.lvV);
  const speed = firstFinite(dashboard?.speed, dashboard?.wheelSpeedAvg);
  const steering = firstFinite(dashboard?.steerColAngle);
  const throttle = firstFinite(dashboard?.throttlePct);
  const brake = firstFinite(dashboard?.brakePct);
  const odometer = firstFinite(liveBanner?.odometer);
  const packetId = firstFinite(dashboard?.packetId);

  const dynamics = sensorData?.dynamics;
  const pack = sensorData?.pack;
  const thermal = sensorData?.thermal;
  const flPot = findNumberByKeys(dynamics, ["flSusPotV", "fl_sus_pot_v"]);
  const frPot = findNumberByKeys(dynamics, ["frSusPotV", "fr_sus_pot_v"]);
  const blPot = findNumberByKeys(dynamics, ["blSusPotV", "bl_sus_pot_v"]);
  const brPot = findNumberByKeys(dynamics, ["brSusPotV", "br_sus_pot_v"]);
  const gpsSpeed = findNumberByKeys(dynamics, ["gpsSpeed", "gps_speed"]);
  const cellTemps =
    findNumberArrayByKeys(pack, ["cellsTemps", "cells_temps", "cellTemps"]) ??
    findNumberArrayByKeys(thermal, ["cellsTemp", "cellsTemps", "cells_temps"]);

  const gps = mapData?.dynamics?.gps;
  const latitude = Array.isArray(gps) ? toFiniteNumber(gps[0]) : undefined;
  const longitude = Array.isArray(gps) ? toFiniteNumber(gps[1]) : undefined;

  const batteryFill = typeof batteryPct === "number" ? clamp(batteryPct, 0, 100) : 0;
  const steerPct =
    typeof steering === "number"
      ? clamp(((clamp(steering, -180, 180) + 180) / 360) * 100, 0, 100)
      : 50;

  const lastSeenSeconds =
    typeof lastMessageAt === "number" ? Math.max(0, Math.floor((Date.now() - lastMessageAt) / 1000)) : undefined;

  return (
    <div
      className="min-h-screen px-4 pb-10 pt-20 text-white md:px-8"
      style={{ background: activeTheme.pageBackground }}
    >
      <div className="mx-auto flex w-full max-w-7xl flex-col gap-6">
        <section className="rounded-3xl border p-6 backdrop-blur-md md:p-8" style={panelStyle}>
          <div className="flex flex-wrap items-start justify-between gap-4">
            <div>
              <p className="text-sm uppercase tracking-[0.22em]" style={{ color: activeTheme.accentSoft }}>
                Longhorn Racing Electric
              </p>
              <h1 className="mt-2 text-3xl font-bold md:text-4xl">Realtime Showcase</h1>
              <p className="mt-2 max-w-3xl text-sm text-white/75 md:text-base">
                Unveiling-grade live telemetry dashboard with core vehicle health, controls, suspension pots, and track
                position in one polished view.
              </p>
            </div>
            <div className="flex w-full max-w-xl flex-col gap-3">
              {multiCarEnabled ? (
                <div className="rounded-xl border p-3" style={{ background: activeTheme.chipBackground, borderColor: activeTheme.chipBorder }}>
                  <label className="mb-1 block text-xs uppercase tracking-wide text-white/70">Live Car</label>
                  <select
                    value={selectedCar}
                    onChange={(evt) => setSelectedCar(evt.target.value as LiveCar)}
                    className="rounded-md border px-3 py-2 text-sm font-medium text-white"
                    style={{ borderColor: activeTheme.accent, background: "rgba(0,0,0,0.38)" }}
                  >
                    {SUPPORTED_LIVE_CARS.map((car) => (
                      <option key={car} value={car}>
                        {liveCarLabel(car)}
                      </option>
                    ))}
                  </select>
                </div>
              ) : (
                <div className="rounded-xl border px-4 py-3 text-sm" style={{ background: activeTheme.chipBackground, borderColor: activeTheme.chipBorder }}>
                  <span className="text-white/70">Live Car</span>
                  <div className="text-base font-semibold">{selectedCarLabel}</div>
                </div>
              )}

              <div className="rounded-xl border p-3" style={{ background: activeTheme.chipBackground, borderColor: activeTheme.chipBorder }}>
                <label className="mb-2 block text-xs uppercase tracking-wide text-white/70">Theme Style</label>
                <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
                  {SHOWCASE_THEMES.map((theme) => {
                    const selected = theme.id === activeTheme.id;
                    return (
                      <button
                        key={theme.id}
                        type="button"
                        onClick={() => setThemeId(theme.id)}
                        className="rounded-lg border px-3 py-2 text-left transition hover:brightness-110"
                        style={{
                          borderColor: selected ? theme.accentSoft : "rgba(255,255,255,0.2)",
                          background: selected ? "rgba(255,255,255,0.1)" : "rgba(0,0,0,0.25)",
                        }}
                      >
                        <div className="flex items-center gap-2 text-sm font-semibold">
                          <span className="inline-block h-2.5 w-2.5 rounded-full" style={{ background: theme.accent }} />
                          {theme.label}
                        </div>
                        <div className="mt-1 text-[11px] text-white/70">{theme.description}</div>
                      </button>
                    );
                  })}
                </div>
              </div>
            </div>
          </div>

          <div className="mt-6 grid grid-cols-2 gap-3 md:grid-cols-5">
            <div className="rounded-xl border p-3" style={cardStyle}>
              <div className="text-xs uppercase tracking-wide text-white/65">Connection</div>
              <div className="mt-1 flex items-center gap-2">
                <span className={`h-2.5 w-2.5 rounded-full ${kafkaConnected ? "bg-emerald-400" : "bg-rose-400"}`} />
                <span className="text-sm font-semibold">{kafkaConnected ? "Live Feed" : "Stale / Offline"}</span>
              </div>
            </div>
            <div className="rounded-xl border p-3" style={cardStyle}>
              <div className="text-xs uppercase tracking-wide text-white/65">Selected Car</div>
              <div className="mt-1 text-sm font-semibold">{selectedCarLabel}</div>
            </div>
            <div className="rounded-xl border p-3" style={cardStyle}>
              <div className="text-xs uppercase tracking-wide text-white/65">Packet ID</div>
              <div className="mt-1 text-sm font-semibold">{formatInt(packetId)}</div>
            </div>
            <div className="rounded-xl border p-3" style={cardStyle}>
              <div className="text-xs uppercase tracking-wide text-white/65">Odometer</div>
              <div className="mt-1 text-sm font-semibold">{formatNumber(odometer, 2)} mi</div>
            </div>
            <div className="rounded-xl border p-3" style={cardStyle}>
              <div className="text-xs uppercase tracking-wide text-white/65">Last Sample</div>
              <div className="mt-1 text-sm font-semibold">
                {typeof lastSeenSeconds === "number" ? `${lastSeenSeconds}s ago` : "No samples"}
              </div>
            </div>
          </div>
        </section>

        <section className="grid grid-cols-1 gap-6 lg:grid-cols-12">
          <div className="rounded-3xl border p-5 lg:col-span-4" style={panelStyle}>
            <h2 className="text-lg font-semibold">Battery & Powertrain</h2>
            <div className="mt-5 flex items-center justify-center">
              <div
                className="relative h-44 w-44 rounded-full p-3"
                style={{
                  background: `conic-gradient(${activeTheme.accent} ${batteryFill}%, rgba(255,255,255,0.12) 0%)`,
                }}
              >
                <div className="flex h-full w-full flex-col items-center justify-center rounded-full bg-[#140B08] ring-1 ring-white/10">
                  <span className={`text-4xl font-bold ${statusTone(batteryPct)}`}>{formatInt(batteryPct)}</span>
                  <span className="text-xs uppercase tracking-wider text-white/65">SOC %</span>
                </div>
              </div>
            </div>
            <div className="mt-5 grid grid-cols-2 gap-3 text-sm">
              <div className="rounded-xl border p-3" style={cardStyle}>
                <div className="text-white/65">HV Pack</div>
                <div className="mt-1 text-lg font-semibold">{formatNumber(hvPackV)} V</div>
              </div>
              <div className="rounded-xl border p-3" style={cardStyle}>
                <div className="text-white/65">HV Current</div>
                <div className="mt-1 text-lg font-semibold">{formatNumber(hvCurrent)} A</div>
              </div>
              <div className="rounded-xl border p-3" style={cardStyle}>
                <div className="text-white/65">Power</div>
                <div className="mt-1 text-lg font-semibold">{formatNumber(powerKw)} kW</div>
              </div>
              <div className="rounded-xl border p-3" style={cardStyle}>
                <div className="text-white/65">LV Battery</div>
                <div className="mt-1 text-lg font-semibold">{formatNumber(lvV)} V</div>
              </div>
            </div>
          </div>

          <div className="rounded-3xl border p-5 lg:col-span-4" style={panelStyle}>
            <h2 className="text-lg font-semibold">Driver Inputs</h2>
            <div className="mt-4 rounded-xl border p-3" style={cardStyle}>
              <div className="mb-2 flex items-center justify-between text-sm">
                <span className="text-white/75">Steering Angle</span>
                <span className="font-semibold">{formatNumber(steering)}&deg;</span>
              </div>
              <div className="h-2 overflow-hidden rounded-full bg-white/15">
                <div
                  className="h-full rounded-full"
                  style={{
                    width: `${steerPct}%`,
                    background: `linear-gradient(90deg, ${activeTheme.accent} 0%, ${activeTheme.accentSoft} 100%)`,
                  }}
                />
              </div>
            </div>

            <div className="mt-4 space-y-4">
              <BarMetric
                label="Throttle"
                value={throttle}
                max={100}
                fill={activeTheme.throttleGradient}
              />
              <BarMetric
                label="Brake"
                value={brake}
                max={100}
                fill={activeTheme.brakeGradient}
              />
            </div>

            <div className="mt-5 grid grid-cols-2 gap-3 text-sm">
              <div className="rounded-xl border p-3" style={cardStyle}>
                <div className="text-white/65">Vehicle Speed</div>
                <div className="mt-1 text-xl font-semibold">{formatNumber(speed)} mph</div>
              </div>
              <div className="rounded-xl border p-3" style={cardStyle}>
                <div className="text-white/65">GPS Speed</div>
                <div className="mt-1 text-xl font-semibold">{formatNumber(gpsSpeed)} mph</div>
              </div>
            </div>
          </div>

          <div className="rounded-3xl border p-5 lg:col-span-4" style={panelStyle}>
            <h2 className="text-lg font-semibold">Suspension Pot Voltages</h2>
            <p className="mt-1 text-xs text-white/60">Live corner potentiometer values from incoming sensor packets.</p>
            <div className="mt-4 grid grid-cols-2 gap-3">
              <SuspensionPot corner="FL" value={flPot} theme={activeTheme} />
              <SuspensionPot corner="FR" value={frPot} theme={activeTheme} />
              <SuspensionPot corner="BL" value={blPot} theme={activeTheme} />
              <SuspensionPot corner="BR" value={brPot} theme={activeTheme} />
            </div>
          </div>
        </section>

        <section className="grid grid-cols-1 gap-6 lg:grid-cols-12">
          <div className="overflow-hidden rounded-3xl border p-2 lg:col-span-7" style={panelStyle}>
            <div className="h-[420px] overflow-hidden rounded-2xl">
              <LiveMap />
            </div>
          </div>

          <div className="rounded-3xl border p-5 lg:col-span-5" style={panelStyle}>
            <h2 className="text-lg font-semibold">Battery Cell Temperature Map</h2>
            <p className="mt-1 text-xs text-white/60">
              Live thermal distribution across pack cells. Orion packets usually expose 90 cell temps.
            </p>
            <div className="mt-4">
              <CellHeatmap values={cellTemps} theme={activeTheme} />
            </div>
          </div>
        </section>

        <section className="grid grid-cols-1 gap-6 lg:grid-cols-12">
          <div className="rounded-3xl border p-5 lg:col-span-12" style={panelStyle}>
            <h2 className="text-lg font-semibold">Position & Thermal Snapshot</h2>
            <div className="mt-4 space-y-3 text-sm">
              <div className="grid grid-cols-1 gap-3 md:grid-cols-3">
                <div className="rounded-xl border p-3" style={cardStyle}>
                  <div className="text-white/65">Latitude</div>
                  <div className="mt-1 text-base font-semibold">{formatNumber(latitude, 6)}</div>
                </div>
                <div className="rounded-xl border p-3" style={cardStyle}>
                  <div className="text-white/65">Longitude</div>
                  <div className="mt-1 text-base font-semibold">{formatNumber(longitude, 6)}</div>
                </div>
                <div className="rounded-xl border p-3" style={cardStyle}>
                  <div className="text-white/65">Time Since On</div>
                  <div className="mt-1 text-base font-semibold">{formatNumber(firstFinite(energy?.timeSinceOnS), 0)} s</div>
                </div>
              </div>
              <div className="grid grid-cols-1 gap-3 md:grid-cols-3">
                <div className="rounded-xl border p-3" style={cardStyle}>
                  <div className="text-white/65">Ambient</div>
                  <div className="mt-1 text-base font-semibold">{formatNumber(firstFinite(dashboard?.ambientTempC))}&deg;C</div>
                </div>
                <div className="rounded-xl border p-3" style={cardStyle}>
                  <div className="text-white/65">Inverter</div>
                  <div className="mt-1 text-base font-semibold">{formatNumber(firstFinite(dashboard?.inverterTempC))}&deg;C</div>
                </div>
                <div className="rounded-xl border p-3" style={cardStyle}>
                  <div className="text-white/65">Motor</div>
                  <div className="mt-1 text-base font-semibold">{formatNumber(firstFinite(dashboard?.motorTempC))}&deg;C</div>
                </div>
              </div>
              <div className="rounded-xl border p-3" style={cardStyle}>
                <div className="text-white/65">Theme Active</div>
                <div className="mt-1 flex items-center gap-2 text-base font-semibold">
                  <span className="inline-block h-2.5 w-2.5 rounded-full" style={{ background: activeTheme.accent }} />
                  {activeTheme.label}
                </div>
              </div>
            </div>
          </div>
        </section>
      </div>
    </div>
  );
}
