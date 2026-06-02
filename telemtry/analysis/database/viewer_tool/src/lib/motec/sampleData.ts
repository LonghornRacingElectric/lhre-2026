// Port of sample_data.py — synthetic Orion telemetry so the tool runs without DB creds.

import { Channel, DataLog, type Sample } from "./datalog";
import { getChannel, ORION_CHANNELS } from "./channels";
import { detectSplitRanges } from "./split";
import type { DriveDay, GpsPoint, SegmentSummary, SeriesPoint, SessionSummary } from "./types";

const BASE_MS = Date.UTC(2026, 3, 25, 14, 0, 0); // 2026-04-25 14:00 UTC
const SESSION_LENGTH_MS = 12 * 60 * 1000;
const STEP_MS = 100;

function value(channelKey: string, elapsedS: number): number {
  const run = (elapsedS >= 60 && elapsedS <= 260) || (elapsedS >= 350 && elapsedS <= 610);
  const wave = Math.sin(elapsedS / 8);
  if (channelKey === "motor_rpm") return run ? 4200 + 1600 * wave + 400 * Math.sin(elapsedS / 1.7) : 250 * Math.max(0, Math.sin(elapsedS / 9));
  if (channelKey === "rpm_request") return run ? 5000 : 0;
  if (["wheel_speed", "gps_speed", "flw_speed", "frw_speed", "blw_speed", "brw_speed"].includes(channelKey)) return run ? 18 + 5 * wave : 0;
  if (channelKey === "apps1_travel") return run ? Math.max(0, 55 + 35 * wave) : 0;
  if (channelKey === "apps2_travel") return run ? Math.max(0, 54 + 34 * wave) : 0;
  if (channelKey === "brake_pressure_f") return run ? Math.max(0, 350 * Math.sin(elapsedS / 13)) : 0;
  if (channelKey.includes("torque")) return run ? 80 + 25 * wave : 0;
  if (channelKey === "steer_col_angle") return 35 * Math.sin(elapsedS / 5.5);
  if (channelKey === "hv_pack_v" || channelKey === "dc_bus_v") return 420 - 8 * Math.sin(elapsedS / 22);
  if (channelKey === "hv_c") return run ? 35 + 20 * wave : 4;
  if (channelKey === "hv_soc") return 92 - elapsedS / 1000;
  if (channelKey.includes("temp")) return 32 + Math.min(45, elapsedS / 18) + 2 * wave;
  return wave;
}

export function listDays(): DriveDay[] {
  return [{ date: "2026-04-25", sessions: 2, start_ms: BASE_MS, end_ms: BASE_MS + SESSION_LENGTH_MS, label: "Sample Orion drive day" }];
}

export function listSessions(date: string): SessionSummary[] {
  if (date !== "2026-04-25") return [];
  return [{
    id: "sample-session-1", label: "Sample Session 1",
    start_ms: BASE_MS, end_ms: BASE_MS + SESSION_LENGTH_MS,
    duration_s: SESSION_LENGTH_MS / 1000, source: "sample", preview_safe: true, warning: null,
  }];
}

export function series(channelKey: string, startMs: number, endMs: number, maxPoints = 5000): SeriesPoint[] {
  const points: SeriesPoint[] = [];
  const span = Math.max(1, endMs - startMs);
  const step = Math.max(STEP_MS, Math.ceil(span / maxPoints));
  for (let t = startMs; t <= endMs; t += step) points.push({ t, v: value(channelKey, (t - BASE_MS) / 1000) });
  return points;
}

export function gps(startMs: number, endMs: number, maxPoints = 2000): GpsPoint[] {
  const points: GpsPoint[] = [];
  const span = Math.max(1, endMs - startMs);
  const step = Math.max(250, Math.ceil(span / maxPoints));
  const centerLat = 30.3922;
  const centerLon = -97.7287;
  for (let t = startMs; t <= endMs; t += step) {
    const elapsed = (t - BASE_MS) / 1000;
    const theta = elapsed / 34;
    points.push({
      t,
      lat: centerLat + 0.0011 * Math.sin(theta) + 0.0002 * Math.sin(theta * 3),
      lon: centerLon + 0.0018 * Math.cos(theta),
    });
  }
  return points;
}

export function datalog(channelKeys: string[], startMs: number, endMs: number): DataLog {
  const log = new DataLog("orion_sample", { source: "sample" });
  for (const key of channelKeys) {
    const def = getChannel(ORION_CHANNELS, key);
    const samples: Sample[] = series(key, startMs, endMs, Math.max(1, Math.floor((endMs - startMs) / STEP_MS))).map((p) => ({
      t: (p.t - startMs) / 1000,
      value: p.v ?? 0,
    }));
    log.channels.set(key, new Channel(def.label, def.unit, def.quantity, samples));
  }
  return log;
}

export function autoSegments(startMs: number, endMs: number, channelKey = "motor_rpm"): SegmentSummary[] {
  const def = getChannel(ORION_CHANNELS, channelKey);
  const samples: Sample[] = series(channelKey, startMs, endMs, Math.max(1, Math.floor((endMs - startMs) / STEP_MS))).map((p) => ({
    t: (p.t - startMs) / 1000,
    value: p.v ?? 0,
  }));
  const ranges = detectSplitRanges(new Channel(def.label, def.unit, def.quantity, samples));
  return ranges.map(([a, b], index) => ({
    id: `auto-${index + 1}`,
    label: `Auto split ${index + 1}`,
    start_ms: startMs + Math.floor(a * 1000),
    end_ms: startMs + Math.floor(b * 1000),
    duration_s: b - a,
    source_channel: channelKey,
    has_gps: true,
    gps_points: Math.max(1, Math.floor((b - a) * 4)),
  }));
}
