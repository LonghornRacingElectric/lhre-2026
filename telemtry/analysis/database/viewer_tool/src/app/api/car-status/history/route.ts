export const runtime = "nodejs";
export const dynamic = "force-dynamic";

import { NextRequest, NextResponse } from "next/server";
import { getSettings } from "@/lib/motec/config";
import { ReadOnlyDatabase } from "@/lib/motec/db";

// Returns car-status segments for a car over a time range, plus derived
// "moving windows" and totals. Self-contained: queried by car + time only
// (no drive-day concept). Uses the read-only pg pool (same per-car DB access as
// the MoTeC layer) — independent of the prisma client.
type SegmentRow = {
  segment_id: string | number | bigint;
  car: string;
  state: string;
  start_time: string | number | bigint;
  end_time: string | number | bigint | null;
  hv_soc_avg: number | null;
  lv_v_avg: number | null;
  active_faults: string | null;
};

const toNum = (v: string | number | bigint | null): number | null =>
  v === null ? null : Number(v);

export async function GET(req: NextRequest) {
  const url = new URL(req.url);
  const carParam = (url.searchParams.get("car") || "orion").trim().toLowerCase();
  const car = carParam === "angelique" ? "angelique" : "orion";

  const now = Date.now();
  const toMs = Number(url.searchParams.get("to")) || now;
  const fromMs = Number(url.searchParams.get("from")) || toMs - 24 * 60 * 60 * 1000; // last 24h
  const limit = Math.min(Math.max(Number(url.searchParams.get("limit")) || 2000, 1), 10000);

  let rows: SegmentRow[];
  try {
    const db = new ReadOnlyDatabase(getSettings(car));
    rows = await db.query<SegmentRow>(
      `select segment_id, car, state, start_time, end_time, hv_soc_avg, lv_v_avg, active_faults
       from car_status_segment
       where car = $1
         and start_time <= $2
         and (end_time is null or end_time >= $3)
       order by start_time asc
       limit $4`,
      [car, Math.floor(toMs), Math.floor(fromMs), limit],
    );
  } catch (e) {
    return NextResponse.json(
      { error: "car_status history unavailable.", detail: String(e instanceof Error ? e.message : e) },
      { status: 503 },
    );
  }

  const segments = rows.map((r) => {
    const start = Number(r.start_time);
    const end = r.end_time === null ? null : Number(r.end_time);
    return {
      id: Number(r.segment_id),
      car: r.car,
      state: r.state,
      startMs: start,
      endMs: end,
      durationMs: end === null ? null : Math.max(0, end - start),
      hvSocAvg: toNum(r.hv_soc_avg),
      lvVAvg: toNum(r.lv_v_avg),
      activeFaults: r.active_faults ? r.active_faults.split(",").filter(Boolean) : [],
    };
  });

  const movingWindows = segments.filter((s) => s.state === "MOVING");
  const totalMovingMs = movingWindows.reduce((sum, s) => sum + (s.durationMs ?? 0), 0);

  const byState: Record<string, number> = {};
  for (const s of segments) {
    if (s.durationMs != null) byState[s.state] = (byState[s.state] ?? 0) + s.durationMs;
  }

  return NextResponse.json({
    car,
    fromMs,
    toMs,
    segments,
    movingWindows: movingWindows.map((s) => ({ startMs: s.startMs, endMs: s.endMs, durationMs: s.durationMs })),
    totals: { movingMs: totalMovingMs, movingCount: movingWindows.length, byState },
  });
}
