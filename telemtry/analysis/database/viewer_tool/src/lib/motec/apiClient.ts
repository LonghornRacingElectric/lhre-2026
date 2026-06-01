// Browser-side API client for the MoTeC exporter routes.
import type {
  ChannelChartDefinition, ChannelDef, DayDetail, DriveDay, ExportRequest, ExportResponse,
  GpsResponse, LiveLapExportRequest, SegmentSummary, SeriesResponse, SourceDef, TrackDefinition,
} from "./types";

async function getJson<T>(url: string): Promise<T> {
  const res = await fetch(url);
  if (!res.ok) throw new Error(await res.text());
  return (await res.json()) as T;
}

async function postJson<T>(url: string, body: unknown): Promise<T> {
  const res = await fetch(url, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) });
  if (!res.ok) throw new Error(await res.text());
  return (await res.json()) as T;
}

const q = (s: string) => encodeURIComponent(s);

export const motecApi = {
  health: () => getJson<{ ok: boolean; source: string; postgres_enabled: boolean }>("/api/motec/health"),
  sources: () => getJson<{ sources: SourceDef[] }>("/api/motec/sources"),
  channels: (source: string) => getJson<{ channels: ChannelDef[]; default: string }>(`/api/motec/channels?source=${q(source)}`),
  calendar: (source: string, channel: string, threshold: number, minDurationS: number, validOnly = true) =>
    getJson<{ days: DriveDay[] }>(
      `/api/motec/calendar?source=${q(source)}&channel=${q(channel)}&threshold=${threshold}&minDurationS=${minDurationS}&validOnly=${validOnly}`,
    ),
  day: (source: string, date: string, channel: string) =>
    getJson<DayDetail>(`/api/motec/day/${q(date)}?source=${q(source)}&channel=${q(channel)}`),
  series: (source: string, channel: string, startMs: number, endMs: number, maxPoints = 5000) =>
    getJson<SeriesResponse>(`/api/motec/series?source=${q(source)}&channel=${q(channel)}&startMs=${startMs}&endMs=${endMs}&maxPoints=${maxPoints}`),
  gps: (source: string, startMs: number, endMs: number, maxPoints = 2000) =>
    getJson<GpsResponse>(`/api/motec/gps?source=${q(source)}&startMs=${startMs}&endMs=${endMs}&maxPoints=${maxPoints}`),
  segments: (source: string, channel: string, startMs: number, endMs: number, threshold: number, minDurationS: number) =>
    getJson<{ segments: SegmentSummary[] }>(
      `/api/motec/segments?source=${q(source)}&channel=${q(channel)}&startMs=${startMs}&endMs=${endMs}&threshold=${threshold}&minDurationS=${minDurationS}`,
    ),
  tracks: () => getJson<{ tracks: TrackDefinition[] }>("/api/motec/tracks"),
  saveTrack: (track: TrackDefinition) => postJson<TrackDefinition>("/api/motec/tracks", track),
  channelCharts: () => getJson<{ charts: ChannelChartDefinition[] }>("/api/motec/channel-charts"),
  export: (body: ExportRequest) => postJson<ExportResponse>("/api/motec/export", body),
  exportLiveLap: (body: LiveLapExportRequest) =>
    postJson<ExportResponse>("/api/motec/live/export-lap", body),
};
