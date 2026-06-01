export type ChannelDef = {
  key: string;
  label: string;
  table: string;
  column: string;
  unit: string;
  quantity: string;
  default: boolean;
  split_candidate: boolean;
};

export type SourceDef = {
  key: "orion" | "angelique";
  label: string;
};

export type KafkaTransport = "local" | "kafka" | "mqtt";

export type DriveDay = {
  date: string;
  sessions: number;
  start_ms: number;
  end_ms: number;
  label: string;
};

export type SessionSummary = {
  id: string;
  label: string;
  start_ms: number;
  end_ms: number;
  duration_s: number;
  source: string;
  preview_safe: boolean;
  warning: string | null;
};

export type SegmentSummary = {
  id: string;
  label: string;
  start_ms: number;
  end_ms: number;
  duration_s: number;
  source_channel: string;
  has_gps: boolean;
  gps_points: number;
};

export type DayDetail = {
  date: string;
  sessions: SessionSummary[];
  segments: SegmentSummary[];
};

export type SegmentResponse = { segments: SegmentSummary[] };

export type SeriesPoint = { t: number; v: number | null };
export type SeriesResponse = { channel: string; label: string; unit: string; points: SeriesPoint[] };
export type GpsPoint = { t: number; lat: number; lon: number };
export type GpsResponse = { points: GpsPoint[] };

export type GateLine = {
  id: string;
  label: string;
  lat1: number;
  lon1: number;
  lat2: number;
  lon2: number;
  role: "start_finish" | "split";
};

export type TrackDefinition = {
  name: string;
  slug: string;
  notes: string;
  gates: GateLine[];
  created_at?: string | null;
  updated_at?: string | null;
};

export type ChannelChartEntry = {
  channel_name: string;
  quantity_type: string;
  unit: string;
  notes: string;
};

export type ChannelChartDefinition = {
  name: string;
  slug: string;
  notes: string;
  entries: ChannelChartEntry[];
  created_at?: string | null;
  updated_at?: string | null;
};

export type ExportResponse = {
  export_id: string;
  zip_path: string;
  files: string[];
};

export type LiveSample = {
  t: number;
  source: string;
  lat: number | null;
  lon: number | null;
  speed: number | null;
  hv_pack_v: number | null;
  hv_c: number | null;
  power_kw: number | null;
  values: Record<string, number>;
};

export type LiveStreamEvent =
  | { type: "status"; ok: boolean; topic: string; transport?: KafkaTransport; message: string; detail?: string }
  | { type: "heartbeat"; topic: string; transport?: KafkaTransport; t: number }
  | { type: "sample"; topic: string; transport?: KafkaTransport; sample: LiveSample };

export type LiveLapExportResponse = ExportResponse & {
  download_url: string;
};
