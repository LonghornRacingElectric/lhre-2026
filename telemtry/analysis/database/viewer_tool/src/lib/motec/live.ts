// Port of live.py (local transport + JSON normalization + live-lap export).
// External Kafka/MQTT broker transports from the reference are out of scope for the
// in-app integration; the local in-process bus path is fully implemented.

import { promises as fs } from "fs";
import path from "path";
import { Channel, DataLog, type Sample } from "./datalog";
import { ORION_CHANNELS } from "./channels";
import type { Settings } from "./config";
import { applyChannelChart, ChannelChartStore } from "./stores";
import { writeLd, writeLdx } from "./motecLd";
import { buildZip } from "./zip";
import type { KafkaTransport, LiveLapExportRequest, LiveSample } from "./types";

export function kafkaTopicFor(source: string, settings: Settings, requested?: string | null): string {
  const t = (requested || "").trim();
  if (t) return t;
  const prefix = (settings as unknown as { kafkaTopicPrefix?: string }).kafkaTopicPrefix?.trim()
    || process.env.KAFKA_TOPIC_PREFIX?.trim()
    || "grafana_data";
  return `${prefix}_${source.trim().toLowerCase()}`;
}

export function kafkaTransportFor(requested?: string | null): KafkaTransport {
  const v = (requested || process.env.KAFKA_MODE || "local").trim().toLowerCase();
  if (["kafka", "broker", "remote"].includes(v)) return "kafka";
  if (["mqtt", "mqtt-broker", "aws"].includes(v)) return "mqtt";
  return "local";
}

function num(value: unknown): number | null {
  const n = typeof value === "number" ? value : Number(value);
  return Number.isFinite(n) ? n : null;
}

function safeKey(value: string): string {
  return [...value].map((c) => (/[a-z0-9]/i.test(c) ? c.toLowerCase() : "_")).join("").replace(/^_+|_+$/g, "") || "value";
}

function first(payload: Record<string, unknown>, paths: string[]): unknown {
  for (const p of paths) {
    let cur: unknown = payload;
    for (const part of p.split(".")) {
      if (!cur || typeof cur !== "object" || !(part in (cur as Record<string, unknown>))) { cur = null; break; }
      cur = (cur as Record<string, unknown>)[part];
    }
    if (cur !== null && cur !== undefined) return cur;
  }
  return null;
}

function flattenNumeric(payload: Record<string, unknown>): Record<string, number> {
  const out: Record<string, number> = {};
  const walk = (prefix: string, value: unknown) => {
    if (value && typeof value === "object" && !Array.isArray(value)) {
      for (const [k, child] of Object.entries(value)) walk(prefix ? `${prefix}_${k}` : k, child);
      return;
    }
    const n = num(value);
    if (n !== null) out[safeKey(prefix)] = n;
  };
  walk("", payload);
  return out;
}

function gpsOf(payload: Record<string, unknown>): [number | null, number | null] {
  const lat = num(first(payload, ["latitude", "lat", "gps_latitude", "dynamics_gps_latitude", "dynamics.latitude"]));
  const lon = num(first(payload, ["longitude", "lon", "lng", "gps_longitude", "dynamics_gps_longitude", "dynamics.longitude"]));
  if (lat !== null && lon !== null) return [lat, lon];
  for (const p of ["gps", "f_gps", "b_gps", "dynamics.gps"]) {
    const pair = first(payload, [p]);
    if (Array.isArray(pair) && pair.length >= 2) {
      const a = num(pair[0]); const b = num(pair[1]);
      if (a !== null && b !== null) return [a, b];
    }
  }
  return [null, null];
}

function timestampMs(value: unknown): number {
  const n = num(value);
  if (n === null || n <= 0) return Date.now();
  if (n < 10_000_000_000) return Math.floor(n * 1000);
  return Math.floor(n);
}

export function normalizeLivePayload(raw: string | Record<string, unknown>, source: string): LiveSample | null {
  let payload: Record<string, unknown>;
  try {
    payload = typeof raw === "string" ? JSON.parse(raw) : raw;
  } catch { return null; }
  if (!payload || typeof payload !== "object" || Array.isArray(payload)) return null;

  const values = flattenNumeric(payload);
  const t = timestampMs(first(payload, ["time", "timestamp", "packet_time", "packet.time"]));
  const [lat, lon] = gpsOf(payload);
  // Prefer a speed source that's actually carrying signal: a flatlined 0 on the
  // top-level `speed` channel used to shadow a real gps/wheel speed (and leaves
  // the live delta bar with nothing to work from). Walk GPS → derived avg →
  // wheel avg → raw, taking the first non-zero; fall back to 0 only if every
  // present source is zero (a genuine standstill / no speed sensor).
  let speed: number | null = null;
  let sawZeroSpeed = false;
  for (const k of ["gps_speed", "dynamics.gps_speed", "dynamics_gps_speed", "gps_velocity", "dash_speed", "wheel_speed_avg", "wheel_speed", "speed"]) {
    const v = num(first(payload, [k]));
    if (v === null) continue;
    if (v > 0) { speed = v; break; }
    sawZeroSpeed = true;
  }
  if (speed === null) {
    const wheels = ["flw_speed", "frw_speed", "blw_speed", "brw_speed"].map((k) => num(first(payload, [k, `dynamics.${k}`]))).filter((v): v is number => v !== null && v > 0);
    if (wheels.length) speed = wheels.reduce((a, b) => a + b, 0) / wheels.length;
    else if (sawZeroSpeed) speed = 0;
  }
  const dcV = num(first(payload, ["dc_bus_v", "bus_voltage", "pack.dc_bus_v", "pack.bus_voltage"]));
  const dcC = num(first(payload, ["dc_bus_current", "pack.dc_bus_current"]));
  const hvVraw = num(first(payload, ["hv_pack_v", "pack_hv_pack_v", "pack.hv_pack_v"]));
  const hvCraw = num(first(payload, ["hv_c", "pack_hv_c", "pack.hv_c"]));
  const hvV = hvVraw ?? dcV;
  const hvC = hvCraw ?? dcC;
  let powerKw = num(first(payload, ["power_kw", "pack.power_kw"]));
  if (powerKw === null && hvV !== null && hvC !== null) powerKw = Math.abs(hvV * hvC) / 1000;
  else if (powerKw !== null) powerKw = Math.abs(powerKw);

  if (speed !== null && values.speed === undefined) values.speed = speed;
  if (hvV !== null && values.hv_pack_v === undefined) values.hv_pack_v = hvV;
  if (hvC !== null && values.hv_c === undefined) values.hv_c = hvC;
  if (powerKw !== null && values.power_kw === undefined) values.power_kw = powerKw;

  return { t, source, lat, lon, speed, hv_pack_v: hvV, hv_c: hvC, power_kw: powerKw, values };
}

// ── Orion-schema enrichment ───────────────────────────────────────────────────
// Our decoded `sensor_data` is a nested camelCase object (dynamics/controls/pack/
// thermal/...) — NOT the flat snake_case the generic normalizeLivePayload expects.
// So its lat/lon/speed/hv lookups miss and come back null. This enricher reads the
// real Orion field paths (mirrors buildNormalizedSensorRoutes in kafkaConsumer.ts)
// and fills the top-level LiveSample fields the dashboard gauges/track-map/lap
// detection rely on. The flattened `values` map (used for MoTeC export) is kept.

function pickNum(obj: unknown, keys: string[]): number | null {
  if (!obj || typeof obj !== "object") return null;
  const rec = obj as Record<string, unknown>;
  for (const k of keys) {
    const n = num(rec[k]);
    if (n !== null) return n;
  }
  return null;
}

function avgNum(values: Array<number | null>): number | null {
  const ok = values.filter((v): v is number => v !== null);
  return ok.length ? ok.reduce((a, b) => a + b, 0) / ok.length : null;
}

/**
 * Given the decoded nested Orion payload and a base sample from
 * normalizeLivePayload, return a sample with correctly-mapped top-level fields.
 * Falls back to the base value whenever a schema-specific field is absent, so
 * non-Orion / already-flat payloads are unaffected.
 */
export function enrichOrionLiveSample(
  payload: Record<string, unknown>,
  base: LiveSample,
): LiveSample {
  const dynamics = (payload.dynamics as Record<string, unknown> | undefined) ?? {};
  const pack = (payload.pack as Record<string, unknown> | undefined) ?? {};

  // GPS: dynamics.gps is a repeated float [lat, lon, (alt)].
  let lat = base.lat;
  let lon = base.lon;
  const gps = dynamics.gps;
  if (Array.isArray(gps) && gps.length >= 2) {
    const a = num(gps[0]);
    const b = num(gps[1]);
    if (a !== null && b !== null && (a !== 0 || b !== 0)) {
      lat = a;
      lon = b;
    }
  }

  // Speed: prefer an explicit speed channel, else average the four wheel speeds.
  const wheelAvg = avgNum([
    pickNum(dynamics, ["flwSpeed"]),
    pickNum(dynamics, ["frwSpeed"]),
    pickNum(dynamics, ["blwSpeed"]),
    pickNum(dynamics, ["brwSpeed"]),
  ]);
  const speed =
    pickNum(dynamics, ["dashSpeed", "wheelSpeed", "gpsSpeed"]) ?? wheelAvg ?? base.speed;

  // HV pack voltage / current.
  const hvV = pickNum(pack, ["hvPackV", "dcBusV", "busVoltage"]) ?? base.hv_pack_v;
  const hvC = pickNum(pack, ["hvC", "dcBusCurrent"]) ?? base.hv_c;

  // Power: explicit, else derived from HV terminal V*I.
  let powerKw = pickNum(pack, ["powerKw"]) ?? base.power_kw;
  if ((powerKw === null || powerKw === 0) && hvV !== null && hvC !== null) {
    powerKw = Math.abs(hvV * hvC) / 1000;
  }

  // Surface the corrected scalars into `values` too so MoTeC export + numeric
  // tiles see them under canonical keys (without clobbering existing entries).
  const values = { ...base.values };
  if (speed !== null) values.speed = speed;
  if (hvV !== null) values.hv_pack_v = hvV;
  if (hvC !== null) values.hv_c = hvC;
  if (powerKw !== null) values.power_kw = powerKw;
  if (lat !== null) values.gps_latitude = lat;
  if (lon !== null) values.gps_longitude = lon;

  return { ...base, lat, lon, speed, hv_pack_v: hvV, hv_c: hvC, power_kw: powerKw, values };
}

// ── Live lap export ───────────────────────────────────────────────────────────

function liveLabel(key: string): string {
  return key.replace(/_/g, " ").replace(/\b\w/g, (c) => c.toUpperCase());
}
function liveUnit(key: string): string {
  const l = key.toLowerCase();
  if (l.includes("speed")) return "m/s";
  if (l.includes("rpm")) return "rpm";
  if (l.endsWith("_v") || l.includes("voltage")) return "V";
  if (l.endsWith("_c") || l.includes("current")) return "A";
  if (l.includes("power")) return "kW";
  if (l.includes("temp")) return "C";
  return "";
}
function liveQuantity(key: string): string {
  const l = key.toLowerCase();
  if (l.includes("speed") || l.includes("rpm")) return "speed";
  if (l.endsWith("_v") || l.includes("voltage")) return "voltage";
  if (l.endsWith("_c") || l.includes("current")) return "current";
  if (l.includes("power")) return "power";
  if (l.includes("temp")) return "temperature";
  return "value";
}

function safeName(value: string): string {
  return [...value].map((c) => (/[a-z0-9._-]/i.test(c) ? c : "_")).join("").replace(/^_+|_+$/g, "") || "live";
}

function datalogFromLiveSamples(samples: Array<{ t: number; values: Record<string, number> }>, startMs: number, car: string): DataLog {
  const defs = new Map(ORION_CHANNELS.map((c) => [c.key, c]));
  const byKey = new Map<string, Sample[]>();
  for (const sample of samples) {
    const elapsed = (sample.t - startMs) / 1000;
    for (const [key, value] of Object.entries(sample.values)) {
      const n = Number(value);
      if (!Number.isFinite(n)) continue;
      if (!byKey.has(key)) byKey.set(key, []);
      byKey.get(key)!.push({ t: elapsed, value: n });
    }
  }
  const log = new DataLog(`${car}_live`, { source: "kafka_live" });
  for (const [key, channelSamples] of byKey) {
    const def = defs.get(key);
    log.channels.set(key, new Channel(
      def ? def.label : liveLabel(key),
      def ? def.unit : liveUnit(key),
      def ? def.quantity : liveQuantity(key),
      channelSamples,
    ));
  }
  return log;
}

export async function exportLiveLap(
  request: LiveLapExportRequest,
  settings: Settings,
  channelCharts?: ChannelChartStore,
): Promise<{ export_id: string; zip_path: string; files: string[]; download_url: string }> {
  if (!request.samples.length) throw new Error("No live samples were supplied.");
  const samples = [...request.samples].sort((a, b) => a.t - b.t);
  const startMs = samples[0].t;
  const endMs = samples[samples.length - 1].t;
  if (endMs <= startMs) throw new Error("Live lap samples must span a positive time range.");

  const exportId = safeName(`live_${request.car}_${request.track_slug || "track"}_latest`);
  const outDir = path.join(settings.exportDir, exportId);
  await fs.mkdir(outDir, { recursive: true });

  const log = datalogFromLiveSamples(samples, startMs, request.car);
  const chart = channelCharts && request.channel_chart_slug ? await channelCharts.load(request.channel_chart_slug).catch(() => null) : null;
  applyChannelChart(log, chart);
  log.resample(request.frequency_hz ?? 50);

  const metadata: Record<string, string> = {
    vehicle_id: request.car.charAt(0).toUpperCase() + request.car.slice(1),
    event: "Live Telemetry",
    session: request.lap_label,
    short_comment: request.lap_label,
    datetime: new Date(startMs).toISOString(),
    ...(request.metadata || {}),
  };
  const stem = safeName(`${request.car.charAt(0).toUpperCase() + request.car.slice(1)}__live_latest`);
  const ld = writeLd(log, metadata);
  const ldx = Buffer.from(writeLdx([0, (endMs - startMs) / 1000], []), "utf8");
  await fs.writeFile(path.join(outDir, `${stem}.ld`), ld);
  await fs.writeFile(path.join(outDir, `${stem}.ldx`), ldx);

  const entries = [{ name: `${stem}.ld`, data: ld }, { name: `${stem}.ldx`, data: ldx }];
  const zip = buildZip(entries);
  const zipPath = path.join(settings.exportDir, `${exportId}.zip`);
  await fs.writeFile(zipPath, zip);

  return {
    export_id: exportId, zip_path: zipPath, files: entries.map((e) => e.name),
    download_url: `/api/motec/export/download?id=${encodeURIComponent(exportId)}`,
  };
}
