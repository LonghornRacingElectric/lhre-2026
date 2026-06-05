// Port of exporter.py — builds per-segment MoTeC/CSV files, zips them under exports/.

import { promises as fs } from "fs";
import path from "path";
import type { Settings } from "./config";
import { DataLog } from "./datalog";
import { writeLd, writeLdx } from "./motecLd";
import { gateCrossingTimes } from "./split";
import { applyChannelChart, ChannelChartStore, TrackStore } from "./stores";
import { TelemetryService } from "./telemetry";
import type { ExportRequest, ExportResponse } from "./types";
import { buildZip } from "./zip";

const SAFE_NAME_RE = /[^A-Za-z0-9_.-]+/g;

function safeName(value: string): string {
  return (value.replace(SAFE_NAME_RE, "_").replace(/^_+|_+$/g, "")) || "export";
}

function stampUtc(ms: number, withTime: boolean): string {
  const d = new Date(ms);
  const p = (n: number) => String(n).padStart(2, "0");
  const base = `${d.getUTCFullYear()}_${p(d.getUTCMonth() + 1)}_${p(d.getUTCDate())}`;
  return withTime ? `${base}__${p(d.getUTCHours())}_${p(d.getUTCMinutes())}_${p(d.getUTCSeconds())}` : base;
}

function csvFloat(value: number): string {
  return Number.isFinite(value) ? String(Math.round(value * 1e9) / 1e9) : "";
}

function writeCsv(log: DataLog, startMs: number): Buffer {
  const channels = [...log.channels.entries()].filter(([, c]) => c.samples.length);
  const seen = new Map<string, number>();
  const headers = ["timestamp_ms", "elapsed_s"];
  for (const [key, channel] of channels) {
    const base = channel.unit ? `${channel.name || key} (${channel.unit})` : `${channel.name || key}`;
    const count = (seen.get(base) || 0) + 1;
    seen.set(base, count);
    headers.push(count === 1 ? base : `${base} ${count}`);
  }
  const maxSamples = channels.reduce((m, [, c]) => Math.max(m, c.samples.length), 0);
  const lines = [headers.map(csvCell).join(",")];
  for (let i = 0; i < maxSamples; i++) {
    let elapsed = 0;
    for (const [, c] of channels) {
      if (i < c.samples.length) { elapsed = c.samples[i].t; break; }
    }
    const row: string[] = [String(Math.round(startMs + elapsed * 1000)), csvFloat(elapsed)];
    for (const [, c] of channels) row.push(i < c.samples.length ? csvFloat(c.samples[i].value) : "");
    lines.push(row.join(","));
  }
  return Buffer.from(lines.join("\r\n") + "\r\n", "utf8");
}

function csvCell(value: string): string {
  return /[",\n]/.test(value) ? `"${value.replace(/"/g, '""')}"` : value;
}

export class Exporter {
  private tracks: TrackStore;
  private channelCharts: ChannelChartStore;

  constructor(private settings: Settings, private telemetry: TelemetryService) {
    this.tracks = new TrackStore(settings.trackDir);
    this.channelCharts = new ChannelChartStore(settings.channelChartDir);
  }

  async export(request: ExportRequest): Promise<ExportResponse> {
    if (!request.segments.length) throw new Error("No export segments were selected.");
    const exportId = await this.exportId(request);
    const outDir = path.join(this.settings.exportDir, exportId);
    await fs.mkdir(outDir, { recursive: true });

    const track =
      request.export_type === "motec" && request.track_slug ? await this.tracks.load(request.track_slug).catch(() => null) : null;
    const channelChart = request.channel_chart_slug
      ? await this.channelCharts.load(request.channel_chart_slug).catch(() => null)
      : null;
    const vehicleName = request.car.charAt(0).toUpperCase() + request.car.slice(1);
    const fileEntries: Array<{ name: string; data: Buffer }> = [];

    let index = 0;
    for (const segment of request.segments) {
      index += 1;
      if (segment.end_ms <= segment.start_ms) throw new Error(`Segment ${segment.id} has an invalid time range.`);
      if (segment.end_ms - segment.start_ms > this.settings.maxExportSeconds * 1000) {
        throw new Error("Export range is too large. Narrow the range before exporting.");
      }
      const log = await this.telemetry.datalog(request.channel_keys, segment.start_ms, segment.end_ms);
      applyChannelChart(log, channelChart);
      log.resample(request.frequency_hz ?? null);

      const stem = safeName(`${vehicleName}__${stampUtc(segment.start_ms, true)}__part${index}`);
      const segMeta = { ...(request.metadata || {}), ...(segment.metadata || {}) };
      const metadata: Record<string, string> = {
        vehicle_id: vehicleName,
        event: segMeta.event || "Telemetry Export",
        session: segMeta.session || segment.label || "",
        short_comment: segMeta.short_comment || segment.id,
        long_comment: segMeta.long_comment || "",
        datetime: new Date(segment.start_ms).toISOString(),
        ...segMeta,
      };
      metadata.session = segMeta.session || segment.label || "";
      metadata.short_comment = segMeta.short_comment || segment.id;

      // Car-status sidecar: tag this export window with the OFF/IDLE/READY/MOVING
      // segments overlapping it, with elapsed-seconds offsets so MoTeC / analysis
      // tooling can jump straight to motion. Best-effort (empty if unavailable).
      const statusSidecar = await this.buildStatusSidecar(request.car, segment.start_ms, segment.end_ms);
      if (statusSidecar) {
        const data = Buffer.from(JSON.stringify(statusSidecar, null, 2), "utf8");
        await fs.writeFile(path.join(outDir, `${stem}.status.json`), data);
        fileEntries.push({ name: `${stem}.status.json`, data });
      }

      if (request.export_type === "csv") {
        const data = writeCsv(log, segment.start_ms);
        await fs.writeFile(path.join(outDir, `${stem}.csv`), data);
        fileEntries.push({ name: `${stem}.csv`, data });
        continue;
      }

      const ld = writeLd(log, metadata);
      await fs.writeFile(path.join(outDir, `${stem}.ld`), ld);
      fileEntries.push({ name: `${stem}.ld`, data: ld });

      const primary: number[] = [];
      const splits: number[] = [];
      if (track) {
        const gps = (await this.telemetry.gps(segment.start_ms, segment.end_ms, 10000)).points;
        const crossings = gateCrossingTimes(gps, track.gates);
        for (const gate of track.gates) {
          const relative = (crossings[gate.id] || []).map((t) => (t - segment.start_ms) / 1000);
          if (gate.role === "start_finish") primary.push(...relative);
          else splits.push(...relative);
        }
      }
      const ldx = Buffer.from(writeLdx(primary, splits), "utf8");
      await fs.writeFile(path.join(outDir, `${stem}.ldx`), ldx);
      fileEntries.push({ name: `${stem}.ldx`, data: ldx });
    }

    const zip = buildZip(fileEntries);
    const zipPath = path.join(this.settings.exportDir, `${exportId}.zip`);
    await fs.writeFile(zipPath, zip);

    return {
      export_id: exportId,
      zip_path: zipPath,
      files: fileEntries.map((f) => f.name),
      download_url: `/api/motec/export/download?id=${encodeURIComponent(exportId)}`,
    };
  }

  // Build the car-status sidecar for one export window. Returns null if there
  // are no overlapping status segments (so we don't write an empty file).
  private async buildStatusSidecar(car: string, startMs: number, endMs: number) {
    const segments = await this.telemetry.carStatusSegments(car, startMs, endMs);
    if (!segments.length) return null;
    const windowMs = Math.max(1, endMs - startMs);
    const clamped = segments.map((s) => {
      const segStart = Math.max(s.startMs, startMs);
      const segEnd = Math.min(s.endMs ?? endMs, endMs);
      return {
        state: s.state,
        start_s: (segStart - startMs) / 1000,        // elapsed seconds into the export
        end_s: (segEnd - startMs) / 1000,
        start_ms: segStart,
        end_ms: segEnd,
        active_faults: s.activeFaults,
      };
    });
    const movingMs = clamped
      .filter((s) => s.state === "MOVING")
      .reduce((sum, s) => sum + (s.end_ms - s.start_ms), 0);
    return {
      car: car.toLowerCase(),
      window_start_ms: startMs,
      window_end_ms: endMs,
      window_duration_s: windowMs / 1000,
      moving_s: movingMs / 1000,
      segments: clamped,
    };
  }

  private async exportId(request: ExportRequest): Promise<string> {
    const firstStart = Math.min(...request.segments.map((s) => s.start_ms));
    const exportDate = stampUtc(firstStart, false);
    const vehicle = safeName((request.metadata?.vehicle_id) || (request.car.charAt(0).toUpperCase() + request.car.slice(1)));
    const base = safeName(`${vehicle}__${exportDate}`);
    let candidate = base;
    let index = 2;
    const exists = async (name: string) =>
      fs.access(path.join(this.settings.exportDir, name)).then(() => true).catch(() => false);
    while ((await exists(candidate)) || (await exists(`${candidate}.zip`))) {
      candidate = `${base}_${index}`;
      index += 1;
    }
    return candidate;
  }
}
