import type {
  ChannelDef,
  ChannelChartDefinition,
  DayDetail,
  DriveDay,
  ExportResponse,
  GpsResponse,
  LiveLapExportResponse,
  SegmentResponse,
  SeriesResponse,
  SourceDef,
  KafkaTransport,
  TrackDefinition,
} from "./types";

async function errorFromResponse(res: Response): Promise<Error> {
  const raw = await res.text();
  try {
    const parsed = JSON.parse(raw);
    if (parsed && typeof parsed.detail === "string") return new Error(parsed.detail);
  } catch {
    // body was not JSON; fall through to raw text
  }
  return new Error(raw || `Request failed (${res.status})`);
}

async function getJson<T>(url: string): Promise<T> {
  const res = await fetch(url);
  if (!res.ok) throw await errorFromResponse(res);
  return res.json() as Promise<T>;
}

async function postJson<T>(url: string, body: unknown): Promise<T> {
  const res = await fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  if (!res.ok) throw await errorFromResponse(res);
  return res.json() as Promise<T>;
}

async function deleteJson<T>(url: string): Promise<T> {
  const res = await fetch(url, { method: "DELETE" });
  if (!res.ok) throw await errorFromResponse(res);
  return res.json() as Promise<T>;
}

export const api = {
  health: () => getJson<{ ok: boolean; source: string; postgres_enabled: boolean }>("/api/motec/health"),
  sources: () => getJson<{ sources: SourceDef[] }>("/api/motec/sources"),
  channels: (source: string) => getJson<{ channels: ChannelDef[]; default: string }>(`/api/motec/channels?source=${encodeURIComponent(source)}`),
  calendar: (source: string, channel: string, threshold: number, minDurationS: number, validOnly = true) =>
    getJson<{ days: DriveDay[] }>(
      `/api/motec/calendar?source=${encodeURIComponent(source)}&channel=${encodeURIComponent(channel)}&threshold=${threshold}&minDurationS=${minDurationS}&validOnly=${validOnly}`,
    ),
  day: (source: string, date: string, channel: string) =>
    getJson<DayDetail>(`/api/motec/day/${date}?source=${encodeURIComponent(source)}&channel=${encodeURIComponent(channel)}`),
  series: (source: string, channel: string, startMs: number, endMs: number, maxPoints = 5000) =>
    getJson<SeriesResponse>(
      `/api/motec/series?source=${encodeURIComponent(source)}&channel=${encodeURIComponent(channel)}&startMs=${startMs}&endMs=${endMs}&maxPoints=${maxPoints}`,
    ),
  gps: (source: string, startMs: number, endMs: number, maxPoints = 2000) =>
    getJson<GpsResponse>(`/api/motec/gps?source=${encodeURIComponent(source)}&startMs=${startMs}&endMs=${endMs}&maxPoints=${maxPoints}`),
  segments: (source: string, channel: string, startMs: number, endMs: number, threshold: number, minDurationS: number) =>
    getJson<SegmentResponse>(
      `/api/motec/segments?source=${encodeURIComponent(source)}&channel=${encodeURIComponent(channel)}&startMs=${startMs}&endMs=${endMs}&threshold=${threshold}&minDurationS=${minDurationS}`,
    ),
  tracks: () => getJson<{ tracks: TrackDefinition[] }>("/api/motec/tracks"),
  saveTrack: (track: TrackDefinition) => postJson<TrackDefinition>("/api/motec/tracks", track),
  channelCharts: () => getJson<{ charts: ChannelChartDefinition[] }>("/api/motec/channel-charts"),
  saveChannelChart: (chart: ChannelChartDefinition) => postJson<ChannelChartDefinition>("/api/motec/channel-charts", chart),
  export: (body: unknown) => postJson<ExportResponse>("/api/motec/export", body),
  liveConfig: (source: string, topic = "", transport: KafkaTransport = "local") =>
    getJson<{ source: string; topic: string; transport: KafkaTransport; bootstrap_servers: string; mqtt_host?: string; mqtt_port?: number }>(
      `/api/motec/live/config?source=${encodeURIComponent(source)}&topic=${encodeURIComponent(topic)}&transport=${encodeURIComponent(transport)}`,
    ),
  exportLiveLap: (body: unknown) => postJson<LiveLapExportResponse>("/api/motec/live/export-lap", body),
  saveLiveSession: (body: unknown) => postJson<{ ok: boolean; savedAt: number }>("/api/motec/live/session-cache", body),
  latestLiveSession: <T>() => getJson<T>("/api/motec/live/session-cache/latest"),
  clearLiveSession: () => deleteJson<{ ok: boolean }>("/api/motec/live/session-cache"),
};
