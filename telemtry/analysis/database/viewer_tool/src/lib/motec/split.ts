// Port of split.py — motion-based auto split detection + GPS gate crossings.

import { Channel } from "./datalog";
import type { GateLine, GpsPoint } from "./types";

export function splitMode(channel: Channel): string {
  const name = channel.name.toLowerCase();
  const unit = channel.unit.toLowerCase();
  if (name.includes("rpm") || unit === "rpm") return "rpm";
  if (name.includes("state") || ["bool", "boolean", "state", "status"].includes(unit)) return "state";
  if (name.includes("speed") || ["m/s", "mph", "kph", "km/h", "rad/s"].includes(unit)) return "speed";
  return "generic";
}

export function motionThreshold(channel: Channel): number {
  if (!channel.samples.length) return 0;
  const peak = Math.max(...channel.samples.map((s) => Math.abs(s.value)));
  const mode = splitMode(channel);
  const unit = channel.unit.toLowerCase();
  if (mode === "state") return 0.5;
  if (mode === "rpm") return Math.max(1200, peak * 0.08);
  if (unit === "mph") return Math.max(3, peak * 0.03);
  if (unit === "m/s") return Math.max(1, peak * 0.03);
  return Math.max(1, peak * 0.03);
}

export function detectActiveRange(channel: Channel): [number, number] | null {
  if (!channel.samples.length) return null;
  const threshold = motionThreshold(channel);
  const mode = splitMode(channel);
  const active: number[] = [];
  for (const sample of channel.samples) {
    const mag = Math.abs(sample.value);
    if (mode === "state" ? mag > threshold : mag >= threshold) active.push(sample.t);
  }
  if (!active.length) return [channel.start, channel.end];
  return [active[0], active[active.length - 1]];
}

export function detectSplitRanges(
  channel: Channel,
  minimumGapS = 12,
  minimumSegmentS = 8,
): Array<[number, number]> {
  const active = detectActiveRange(channel);
  if (active === null) return [];
  const [activeStart, activeEnd] = active;
  const threshold = motionThreshold(channel);
  const mode = splitMode(channel);
  const ranges: Array<[number, number]> = [];
  let segmentStart = activeStart;
  let gapStart: number | null = null;
  let previousT: number | null = null;
  let segmentOpen = true;

  for (const sample of channel.samples) {
    if (sample.t < activeStart) continue;
    if (sample.t > activeEnd) break;
    const stationary = !(mode === "state" ? Math.abs(sample.value) > threshold : Math.abs(sample.value) >= threshold);
    if (stationary && gapStart === null) {
      gapStart = previousT !== null ? previousT : sample.t;
    } else if (!stationary && gapStart !== null) {
      const gapEnd = previousT !== null ? previousT : sample.t;
      if (gapEnd - gapStart >= minimumGapS && gapStart - segmentStart >= minimumSegmentS) {
        ranges.push([segmentStart, gapStart]);
        segmentStart = sample.t;
      }
      gapStart = null;
    }
    previousT = sample.t;
  }

  if (gapStart !== null && previousT !== null) {
    if (previousT - gapStart >= minimumGapS && gapStart - segmentStart >= minimumSegmentS) {
      ranges.push([segmentStart, gapStart]);
      segmentOpen = false;
    }
  }

  if (segmentOpen && activeEnd - segmentStart >= minimumSegmentS) {
    ranges.push([segmentStart, activeEnd]);
  }
  if (!ranges.length && activeEnd > activeStart) return [[activeStart, activeEnd]];
  return normalizeRanges(ranges);
}

export function normalizeRanges(ranges: Array<[number, number]>): Array<[number, number]> {
  const clean = ranges
    .map(([a, b]) => [Number(a), Number(b)] as [number, number])
    .filter(([a, b]) => b > a)
    .sort((x, y) => x[0] - y[0]);
  const merged: Array<[number, number]> = [];
  for (const [start, end] of clean) {
    if (merged.length && start <= merged[merged.length - 1][1]) {
      merged[merged.length - 1][1] = Math.max(merged[merged.length - 1][1], end);
    } else {
      merged.push([start, end]);
    }
  }
  return merged;
}

function orientation(ax: number, ay: number, bx: number, by: number, cx: number, cy: number): number {
  return (by - ay) * (cx - bx) - (bx - ax) * (cy - by);
}

function segmentsCross(
  a: [number, number], b: [number, number], c: [number, number], d: [number, number],
): boolean {
  const o1 = orientation(a[0], a[1], b[0], b[1], c[0], c[1]);
  const o2 = orientation(a[0], a[1], b[0], b[1], d[0], d[1]);
  const o3 = orientation(c[0], c[1], d[0], d[1], a[0], a[1]);
  const o4 = orientation(c[0], c[1], d[0], d[1], b[0], b[1]);
  return o1 * o2 < 0 && o3 * o4 < 0;
}

export function gateCrossingTimes(points: GpsPoint[], gates: GateLine[]): Record<string, number[]> {
  const crossings: Record<string, number[]> = {};
  for (const gate of gates) crossings[gate.id] = [];
  if (points.length < 2) return crossings;
  for (let i = 0; i + 1 < points.length; i++) {
    const prev = points[i];
    const cur = points[i + 1];
    const traceA: [number, number] = [prev.lon, prev.lat];
    const traceB: [number, number] = [cur.lon, cur.lat];
    for (const gate of gates) {
      const gateA: [number, number] = [gate.lon1, gate.lat1];
      const gateB: [number, number] = [gate.lon2, gate.lat2];
      if (segmentsCross(traceA, traceB, gateA, gateB)) crossings[gate.id].push(cur.t);
    }
  }
  return crossings;
}
