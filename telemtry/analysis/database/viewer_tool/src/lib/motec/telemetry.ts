// Port of telemetry.py — calendar/session/segment/series/gps/datalog over sample or Postgres.
// Postgres queries use pg positional params ($1...) instead of psycopg named params.

import { Channel, DataLog, type Sample } from "./datalog";
import { DEFAULT_CHANNEL_KEY, ORION_CHANNELS, getChannel } from "./channels";
import type { Settings } from "./config";
import { ReadOnlyDatabase } from "./db";
import { detectSplitRanges } from "./split";
import * as sample from "./sampleData";
import type {
  ChannelDef, DayDetail, DriveDay, GpsResponse, SegmentSummary, SeriesPoint, SeriesResponse, SessionSummary,
} from "./types";

function mergeMsRanges(ranges: Array<[number, number]>): Array<[number, number]> {
  const clean = ranges.map(([a, b]) => [a, b] as [number, number]).filter(([a, b]) => b > a).sort((x, y) => x[0] - y[0]);
  const merged: Array<[number, number]> = [];
  for (const [start, end] of clean) {
    if (merged.length && start <= merged[merged.length - 1][1] + 1) {
      merged[merged.length - 1][1] = Math.max(merged[merged.length - 1][1], end);
    } else merged.push([start, end]);
  }
  return merged;
}

function dayLabel(date: string, count: number, sessions: boolean): string {
  const noun = sessions ? "session" : "source range";
  return `${date} (${count} ${noun}${count === 1 ? "" : "s"})`;
}

// ── timezone helpers (Intl-based) ─────────────────────────────────────────────

function tzParts(utcMs: number, timeZone: string) {
  const dtf = new Intl.DateTimeFormat("en-US", {
    timeZone, year: "numeric", month: "2-digit", day: "2-digit",
    hour: "2-digit", minute: "2-digit", second: "2-digit", hour12: false,
  });
  const parts: Record<string, number> = {};
  for (const p of dtf.formatToParts(new Date(utcMs))) {
    if (p.type !== "literal") parts[p.type] = parseInt(p.value, 10);
  }
  if (parts.hour === 24) parts.hour = 0;
  return parts;
}

function tzOffsetMs(utcMs: number, timeZone: string): number {
  const p = tzParts(utcMs, timeZone);
  const asUtc = Date.UTC(p.year, p.month - 1, p.day, p.hour, p.minute, p.second);
  return asUtc - utcMs;
}

function localDayBoundsMs(date: string, timeZone: string): [number, number] {
  const [y, m, d] = date.split("-").map(Number);
  const naive = Date.UTC(y, m - 1, d, 0, 0, 0);
  let start = naive - tzOffsetMs(naive, timeZone);
  start = naive - tzOffsetMs(start, timeZone); // refine once for DST edges
  return [start, start + 86_400_000];
}

function localDateOf(utcMs: number, timeZone: string): string {
  const p = tzParts(utcMs, timeZone);
  return `${p.year}-${String(p.month).padStart(2, "0")}-${String(p.day).padStart(2, "0")}`;
}

function datesBetweenLocal(startMs: number, endMs: number, timeZone: string): string[] {
  const startDate = localDateOf(startMs, timeZone);
  const endDate = localDateOf(Math.max(startMs, endMs - 1), timeZone);
  const out: string[] = [];
  const cursor = new Date(`${startDate}T00:00:00Z`);
  const last = new Date(`${endDate}T00:00:00Z`);
  while (cursor.getTime() <= last.getTime()) {
    out.push(cursor.toISOString().slice(0, 10));
    cursor.setUTCDate(cursor.getUTCDate() + 1);
  }
  return out;
}

export class TelemetryService {
  private db: ReadOnlyDatabase;
  private channelCache: ChannelDef[] | null = null;

  constructor(private settings: Settings) {
    this.db = new ReadOnlyDatabase(settings);
  }

  async channels(): Promise<ChannelDef[]> {
    if (!this.settings.usePostgres) return ORION_CHANNELS;
    if (!this.channelCache) this.channelCache = await this.loadChannels();
    return this.channelCache;
  }

  async getChannel(key: string | null | undefined): Promise<ChannelDef> {
    return getChannel(await this.channels(), key);
  }

  async calendar(channelKey: string | null, threshold = 0, minDurationS = 0, validOnly = false): Promise<DriveDay[]> {
    if (!this.settings.usePostgres) return sample.listDays();
    const rows = await this.db.query<{ start_ms: string; end_ms: string }>(
      `select start_time::bigint as start_ms, end_time::bigint as end_ms
       from partitions where end_time > start_time order by end_time desc limit 2000`,
    );
    const rangesByDate: Record<string, Array<[number, number]>> = {};
    for (const row of rows) {
      const startMs = Number(row.start_ms);
      const endMs = Number(row.end_ms);
      for (const date of datesBetweenLocal(startMs, endMs, this.settings.displayTimezone)) {
        const [dayStart, dayEnd] = localDayBoundsMs(date, this.settings.displayTimezone);
        const cs = Math.max(startMs, dayStart);
        const ce = Math.min(endMs, dayEnd);
        if (ce > cs) (rangesByDate[date] ||= []).push([cs, ce]);
      }
    }
    let sessionCounts: Record<string, number> | null = null;
    if (validOnly && channelKey && Object.keys(rangesByDate).length) {
      const all = Object.values(rangesByDate).flat();
      sessionCounts = await this.thresholdSessionCountsByDate(
        channelKey,
        Math.min(...all.map((r) => r[0])),
        Math.max(...all.map((r) => r[1])),
        threshold,
        minDurationS,
      );
    }
    const days: DriveDay[] = [];
    for (const date of Object.keys(rangesByDate).sort().reverse()) {
      const ranges = mergeMsRanges(rangesByDate[date]);
      if (!ranges.length) continue;
      let sessionCount: number | null = null;
      if (validOnly && channelKey) {
        sessionCount = (sessionCounts || {})[date] || 0;
        if (sessionCount === 0) continue;
      }
      const count = sessionCount ?? ranges.length;
      days.push({
        date,
        sessions: count,
        start_ms: Math.min(...ranges.map((r) => r[0])),
        end_ms: Math.max(...ranges.map((r) => r[1])),
        label: dayLabel(date, count, sessionCount !== null),
      });
    }
    return days.slice(0, 180);
  }

  async dayDetail(date: string, channelKey = DEFAULT_CHANNEL_KEY): Promise<DayDetail> {
    const sessions = await this.sessionsForDay(date);
    const segments: SegmentSummary[] = [];
    if (this.settings.usePostgres) return { date, sessions, segments };
    for (const session of sessions) {
      if (session.preview_safe && session.duration_s <= this.settings.maxAutoSplitSeconds) {
        segments.push(...(await this.autoSegments(session.start_ms, session.end_ms, channelKey)));
      }
    }
    return { date, sessions, segments };
  }

  async sessionsForDay(date: string): Promise<SessionSummary[]> {
    if (!this.settings.usePostgres) return sample.listSessions(date);
    const [dayStart, dayEnd] = localDayBoundsMs(date, this.settings.displayTimezone);
    const rows = await this.db.query<{ start_ms: string; end_ms: string }>(
      `select start_time::bigint as start_ms, end_time::bigint as end_ms
       from partitions where start_time < $1 and end_time > $2 and end_time > start_time order by start_time asc`,
      [dayEnd, dayStart],
    );
    const ranges = mergeMsRanges(rows.map((r) => [Math.max(Number(r.start_ms), dayStart), Math.min(Number(r.end_ms), dayEnd)]));
    const sessions: SessionSummary[] = [];
    ranges.forEach(([startMs, endMs], i) => {
      if (endMs <= startMs) return;
      const durationS = (endMs - startMs) / 1000;
      const previewSafe = durationS > 0 && durationS <= this.settings.maxPreviewSeconds;
      let warning: string | null = null;
      if (durationS <= 0) warning = "Zero-duration source range";
      else if (!previewSafe) warning = "Long source range; threshold sessions are generated from this local-day slice";
      sessions.push({
        id: `${date}-${i + 1}-${startMs}-${endMs}`,
        label: `Source range ${i + 1}`,
        start_ms: startMs, end_ms: endMs, duration_s: durationS,
        source: "partition_day_window", preview_safe: previewSafe, warning,
      });
    });
    return sessions;
  }

  async series(channelKey: string, startMs: number, endMs: number, maxPoints?: number): Promise<SeriesResponse> {
    const channel = await this.getChannel(channelKey);
    const mp = maxPoints || this.settings.maxPreviewPoints;
    const points = this.settings.usePostgres
      ? await this.postgresSeries(channel, startMs, endMs, mp)
      : sample.series(channel.key, startMs, endMs, mp);
    return { channel: channel.key, label: channel.label, unit: channel.unit, points };
  }

  async gps(startMs: number, endMs: number, maxPoints = 2000): Promise<GpsResponse> {
    if (!this.settings.usePostgres) return { points: sample.gps(startMs, endMs, maxPoints) };
    const span = Math.max(1, endMs - startMs);
    const stepMs = Math.max(1, Math.floor(span / Math.max(1, maxPoints)));
    const rows = await this.db.query<{ t: string; lat: number | null; lon: number | null }>(
      `select (floor(packet.time::double precision / $1) * $1)::bigint as t,
              avg(dynamics.gps[1]) as lat, avg(dynamics.gps[2]) as lon
       from packet join dynamics on dynamics.packet_id = packet.packet_id
       where packet.time between $2 and $3 and dynamics.gps is not null and array_length(dynamics.gps, 1) >= 2
       group by 1 order by 1`,
      [stepMs, startMs, endMs],
    );
    return {
      points: rows
        .filter((r) => r.lat !== null && r.lon !== null)
        .map((r) => ({ t: Number(r.t), lat: Number(r.lat), lon: Number(r.lon) })),
    };
  }

  async autoSegments(startMs: number, endMs: number, channelKey = DEFAULT_CHANNEL_KEY): Promise<SegmentSummary[]> {
    if (!this.settings.usePostgres) return sample.autoSegments(startMs, endMs, channelKey);
    const series = await this.series(channelKey, startMs, endMs, 20000);
    const channel = await this.getChannel(channelKey);
    const splitChannel = new Channel(
      channel.label, channel.unit, channel.quantity,
      series.points.map((p) => ({ t: (p.t - startMs) / 1000, value: p.v ?? 0 })),
    );
    const ranges = detectSplitRanges(splitChannel);
    const segments: SegmentSummary[] = [];
    ranges.forEach(([a, b], index) => {
      const segStart = Math.max(startMs, startMs + Math.floor(a * 1000));
      const segEnd = Math.min(endMs, startMs + Math.floor(b * 1000));
      if (segEnd <= segStart) return;
      segments.push({
        id: `auto-${startMs}-${index + 1}`, label: `Auto split ${index + 1}`,
        start_ms: segStart, end_ms: segEnd, duration_s: (segEnd - segStart) / 1000,
        source_channel: channelKey, has_gps: false, gps_points: 0,
      });
    });
    return this.withGpsCoverage(segments);
  }

  async thresholdSegments(
    startMs: number, endMs: number, channelKey = DEFAULT_CHANNEL_KEY,
    threshold = 0, maxPoints = 20000, minDurationS = 0,
  ): Promise<SegmentSummary[]> {
    if (!this.settings.usePostgres) {
      return sample.autoSegments(startMs, endMs, channelKey).filter((s) => s.duration_s >= minDurationS);
    }
    if (endMs <= startMs) return [];
    const channel = await this.getChannel(channelKey);
    const points = await this.postgresSeries(channel, startMs, endMs, maxPoints);
    const segments: SegmentSummary[] = [];
    let activeStart: number | null = null;
    let lastT: number | null = null;
    const push = (s: number, e: number) => {
      if (e > s && (e - s) / 1000 >= minDurationS) {
        segments.push({
          id: `auto-${channel.key}-${s}-${segments.length + 1}`, label: `Session ${segments.length + 1}`,
          start_ms: s, end_ms: e, duration_s: (e - s) / 1000, source_channel: channel.key, has_gps: false, gps_points: 0,
        });
      }
    };
    for (const point of points) {
      const isActive = point.v !== null && point.v > threshold;
      if (isActive && activeStart === null) activeStart = Math.max(startMs, point.t);
      if (!isActive && activeStart !== null) {
        push(activeStart, Math.min(endMs, lastT ?? point.t));
        activeStart = null;
      }
      lastT = point.t;
    }
    if (activeStart !== null) push(activeStart, Math.min(endMs, lastT ?? endMs));
    return this.withGpsCoverage(segments);
  }

  async datalog(channelKeys: string[], startMs: number, endMs: number): Promise<DataLog> {
    const selected = channelKeys.length ? channelKeys : ORION_CHANNELS.map((c) => c.key);
    if (!this.settings.usePostgres) return sample.datalog(selected, startMs, endMs);
    const channelByKey = new Map((await this.channels()).map((c) => [c.key, c]));
    const definitions = selected.map((k) => channelByKey.get(k)).filter((d): d is ChannelDef => Boolean(d));
    const tables = [...new Set(definitions.map((d) => d.table))].sort();
    const selectExprs = definitions.map((d) => `${d.table}.${d.column} as ${d.key}`);
    const joins = tables.map((t) => `left join ${t} on ${t}.packet_id = packet.packet_id`).join(" ");
    const rows = await this.db.query<Record<string, unknown>>(
      `select packet.time::bigint as t, ${selectExprs.join(", ")}
       from packet ${joins} where packet.time between $1 and $2 order by packet.time asc`,
      [startMs, endMs],
    );
    const log = new DataLog(`${this.settings.orionDbName}_postgres`, {
      source: "postgres", start_ms: String(startMs), end_ms: String(endMs),
    });
    for (const def of definitions) {
      const samples: Sample[] = [];
      for (const row of rows) {
        const v = row[def.key];
        if (v !== null && v !== undefined) samples.push({ t: (Number(row.t) - startMs) / 1000, value: Number(v) });
      }
      if (samples.length) log.channels.set(def.key, new Channel(def.label, def.unit, def.quantity, samples));
    }
    return log;
  }

  private async postgresSeries(channel: ChannelDef, startMs: number, endMs: number, maxPoints: number): Promise<SeriesPoint[]> {
    const span = Math.max(1, endMs - startMs);
    const stepMs = Math.max(1, Math.floor(span / Math.max(1, maxPoints)));
    const rows = await this.db.query<{ t: string; v: number | null }>(
      `select (floor(packet.time::double precision / $1) * $1)::bigint as t, avg(${channel.table}.${channel.column}) as v
       from packet join ${channel.table} on ${channel.table}.packet_id = packet.packet_id
       where packet.time between $2 and $3 group by 1 order by 1`,
      [stepMs, startMs, endMs],
    );
    return rows.map((r) => ({ t: Number(r.t), v: r.v !== null ? Number(r.v) : null }));
  }

  private async withGpsCoverage(segments: SegmentSummary[]): Promise<SegmentSummary[]> {
    if (!segments.length) return segments;
    if (!this.settings.usePostgres) {
      return segments.map((s) => ({ ...s, has_gps: true, gps_points: Math.max(1, s.gps_points) }));
    }
    const values: string[] = [];
    const params: number[] = [];
    segments.forEach((seg, i) => {
      values.push(`($${i * 3 + 1}, $${i * 3 + 2}, $${i * 3 + 3})`);
      params.push(i, seg.start_ms, seg.end_ms);
    });
    const rows = await this.db.query<{ idx: number; gps_points: string }>(
      `with ranges(idx, start_ms, end_ms) as (values ${values.join(", ")})
       select ranges.idx, count(dynamics.packet_id)::bigint as gps_points
       from ranges
       left join packet on packet.time between ranges.start_ms and ranges.end_ms
       left join dynamics on dynamics.packet_id = packet.packet_id and dynamics.gps is not null
         and array_length(dynamics.gps, 1) >= 2 and dynamics.gps[1] is not null and dynamics.gps[2] is not null
       group by ranges.idx`,
      params,
    );
    const counts = new Map(rows.map((r) => [Number(r.idx), Number(r.gps_points)]));
    return segments.map((seg, i) => ({ ...seg, has_gps: (counts.get(i) || 0) > 0, gps_points: counts.get(i) || 0 }));
  }

  private async thresholdSessionCountsByDate(
    channelKey: string, startMs: number, endMs: number, threshold: number, minDurationS: number,
  ): Promise<Record<string, number>> {
    const channel = await this.getChannel(channelKey);
    const stepMs = Math.max(1, Math.floor((24 * 60 * 60 * 1000) / 20000));
    const rows = await this.db.query<{ date: string; t: string }>(
      `select to_char(to_timestamp(packet.time / 1000.0) at time zone $1, 'YYYY-MM-DD') as date,
              (floor(packet.time::double precision / $2) * $2)::bigint as t
       from packet join ${channel.table} on ${channel.table}.packet_id = packet.packet_id
       where packet.time between $3 and $4 group by 1, 2
       having avg(${channel.table}.${channel.column}) > $5 order by 1, 2`,
      [this.settings.displayTimezone, stepMs, startMs, endMs, threshold],
    );
    const counts: Record<string, number> = {};
    const activeStart: Record<string, number> = {};
    const lastT: Record<string, number> = {};
    const gapMs = stepMs * 1.5;
    for (const row of rows) {
      const date = row.date;
      const t = Number(row.t);
      if (activeStart[date] === undefined || lastT[date] === undefined) {
        activeStart[date] = t; lastT[date] = t; continue;
      }
      if (t - lastT[date] > gapMs) {
        if ((lastT[date] - activeStart[date]) / 1000 >= minDurationS) counts[date] = (counts[date] || 0) + 1;
        activeStart[date] = t;
      }
      lastT[date] = t;
    }
    for (const date of Object.keys(activeStart)) {
      if ((lastT[date] - activeStart[date]) / 1000 >= minDurationS) counts[date] = (counts[date] || 0) + 1;
    }
    return counts;
  }

  private async loadChannels(): Promise<ChannelDef[]> {
    const rows = await this.db.query<{ table_name: string; column_name: string; data_type: string }>(
      `select table_name, column_name, data_type from information_schema.columns
       where table_schema='public'
         and table_name in ('controls','dynamics','pack','thermal','diagnostics','diagnostics_high','diagnostics_low')
         and column_name <> 'packet_id' order by table_name, ordinal_position`,
    );
    const channels: ChannelDef[] = [];
    for (const row of rows) {
      const { table_name: table, column_name: column, data_type: dataType } = row;
      if (["real", "double precision", "integer", "bigint", "smallint", "numeric"].includes(dataType)) {
        const key = `${table}__${column}`.toLowerCase().replace(/[^a-z0-9]+/g, "_").replace(/^_+|_+$/g, "");
        channels.push({
          key, label: labelFor(column), table, column, unit: unitFor(column), quantity: quantityFor(column),
          default: ["controls_motor_speed", "dynamics_inverter_rpm"].includes(key), split_candidate: isSplitCandidate(column),
        });
      } else if (dataType === "ARRAY" && column === "gps") {
        channels.push(
          { key: "dynamics_gps_latitude", label: "GPS Latitude", table, column: "gps[1]", unit: "deg", quantity: "position", default: false, split_candidate: false },
          { key: "dynamics_gps_longitude", label: "GPS Longitude", table, column: "gps[2]", unit: "deg", quantity: "position", default: false, split_candidate: false },
        );
      }
    }
    if (!channels.some((c) => c.default) && channels.length) channels[0].default = true;
    return channels.length ? channels : ORION_CHANNELS;
  }
}

function labelFor(column: string): string {
  return column === "rpm_request"
    ? column.replace(/_/g, " ").toUpperCase()
    : column.replace(/_/g, " ").replace(/\b\w/g, (c) => c.toUpperCase());
}

function unitFor(column: string): string {
  const l = column.toLowerCase();
  if (l.includes("rpm") || l === "motor_speed") return "rpm";
  if (l.endsWith("_speed") && l.includes("wheel")) return "rad/s";
  if (l.endsWith("_v") || l.includes("voltage")) return "V";
  if (l.endsWith("_c") || l.includes("current")) return "A";
  if (l.includes("temp")) return "C";
  if (l.includes("pressure")) return "psi";
  if (l.includes("torque")) return "Nm";
  if (l.includes("gps") && l.includes("speed")) return "m/s";
  return "";
}

function quantityFor(column: string): string {
  const l = column.toLowerCase();
  if (l.includes("speed") || l.includes("rpm")) return "speed";
  if (l.includes("temp")) return "temperature";
  if (l.includes("pressure")) return "pressure";
  if (l.includes("torque")) return "torque";
  if (l.endsWith("_v") || l.includes("voltage")) return "voltage";
  if (l.endsWith("_c") || l.includes("current")) return "current";
  return "value";
}

function isSplitCandidate(column: string): boolean {
  const l = column.toLowerCase();
  return ["rpm", "speed", "torque", "apps", "gps_velocity"].some((t) => l.includes(t));
}
