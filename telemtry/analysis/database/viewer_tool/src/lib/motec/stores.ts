// Port of track_store.py + channel_chart_store.py — local JSON persistence.

import { promises as fs } from "fs";
import path from "path";
import type { ChannelChartDefinition, TrackDefinition } from "./types";
import type { DataLog } from "./datalog";

export function slugify(value: string, fallback: string): string {
  const slug = (value || "").toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "");
  return slug || fallback;
}

async function ensureDir(dir: string): Promise<void> {
  await fs.mkdir(dir, { recursive: true });
}

async function listJson<T>(dir: string, loader: (slug: string) => Promise<T>): Promise<T[]> {
  await ensureDir(dir);
  const entries = (await fs.readdir(dir)).filter((f) => f.endsWith(".json")).sort();
  const out: T[] = [];
  for (const entry of entries) out.push(await loader(entry.replace(/\.json$/, "")));
  return out;
}

export class TrackStore {
  constructor(private root: string) {}

  list(): Promise<TrackDefinition[]> {
    return listJson(this.root, (slug) => this.load(slug));
  }

  async load(slug: string): Promise<TrackDefinition> {
    const raw = await fs.readFile(path.join(this.root, `${slugify(slug, "track")}.json`), "utf8");
    return JSON.parse(raw) as TrackDefinition;
  }

  async save(track: TrackDefinition): Promise<TrackDefinition> {
    await ensureDir(this.root);
    const now = new Date().toISOString();
    const slug = slugify(track.slug || track.name, "track");
    const file = path.join(this.root, `${slug}.json`);
    let createdAt = track.created_at;
    if (!createdAt) {
      try { createdAt = (await this.load(slug)).created_at; } catch { createdAt = now; }
    }
    const out: TrackDefinition = { ...track, slug, created_at: createdAt || now, updated_at: now };
    await fs.writeFile(file, JSON.stringify(out, null, 2) + "\n", "utf8");
    return out;
  }
}

export function normalizeChannelName(value: string): string {
  return (value || "").trim().toLowerCase().split(/\s+/).join(" ");
}

export function applyChannelChart(dataLog: DataLog, chart: ChannelChartDefinition | null): number {
  if (!chart) return 0;
  const lookup = new Map(
    chart.entries.filter((e) => e.channel_name.trim()).map((e) => [normalizeChannelName(e.channel_name), e]),
  );
  let matched = 0;
  for (const channel of dataLog.channels.values()) {
    const entry = lookup.get(normalizeChannelName(channel.name));
    if (!entry) continue;
    if (entry.quantity_type) channel.quantity = entry.quantity_type;
    if (entry.unit) channel.unit = entry.unit;
    matched += 1;
  }
  return matched;
}

export class ChannelChartStore {
  constructor(private root: string) {}

  list(): Promise<ChannelChartDefinition[]> {
    return listJson(this.root, (slug) => this.load(slug));
  }

  async load(slug: string): Promise<ChannelChartDefinition> {
    const raw = await fs.readFile(path.join(this.root, `${slugify(slug, "channel-chart")}.json`), "utf8");
    return JSON.parse(raw) as ChannelChartDefinition;
  }

  async save(chart: ChannelChartDefinition): Promise<ChannelChartDefinition> {
    await ensureDir(this.root);
    const now = new Date().toISOString();
    const slug = slugify(chart.slug || chart.name, "channel-chart");
    const file = path.join(this.root, `${slug}.json`);
    let createdAt = chart.created_at;
    if (!createdAt) {
      try { createdAt = (await this.load(slug)).created_at; } catch { createdAt = now; }
    }
    const out: ChannelChartDefinition = { ...chart, slug, created_at: createdAt || now, updated_at: now };
    await fs.writeFile(file, JSON.stringify(out, null, 2) + "\n", "utf8");
    return out;
  }
}
