// Shared construction of MoTeC services from a `source` query param.
import { getSettings } from "./config";
import { TelemetryService } from "./telemetry";
import { Exporter } from "./exporter";
import { ChannelChartStore, TrackStore } from "./stores";
import type { SourceDef } from "./types";

export const SOURCES: SourceDef[] = [
  { key: "orion", label: "Orion" },
  { key: "angelique", label: "Angelique" },
];

export function services(source?: string | null) {
  const settings = getSettings(source);
  const telemetry = new TelemetryService(settings);
  return {
    settings,
    telemetry,
    exporter: new Exporter(settings, telemetry),
    tracks: new TrackStore(settings.trackDir),
    channelCharts: new ChannelChartStore(settings.channelChartDir),
  };
}

export function num(value: string | null, fallback: number): number {
  const n = Number(value);
  return Number.isFinite(n) ? n : fallback;
}
