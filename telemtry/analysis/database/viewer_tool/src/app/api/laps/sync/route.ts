// Sync a drive day's laps into the classifier table (type='lap'), so trackside-
// live's Live Laps (with edited times + per-lap notes) are DB-backed like the
// Driveday tool. Idempotent "replace the set" model: delete this day's lap rows
// and re-insert the current set — one path covers add / edit-time / edit-note /
// delete. Same Prisma telemetry-client + classifier model as /api/event-flag.
export const runtime = "nodejs";
export const dynamic = "force-dynamic";

import { NextRequest, NextResponse } from "next/server";
import prisma from "@/lib/prisma/telemtry";

interface LapIn { start_time?: unknown; end_time?: unknown; notes?: unknown }

export async function POST(req: NextRequest) {
  try {
    const body = (await req.json().catch(() => ({}))) as { day_id?: unknown; laps?: unknown };
    const dayId = Number(body.day_id);
    if (!Number.isFinite(dayId)) {
      return NextResponse.json({ error: "missing day_id" }, { status: 400 });
    }
    const laps = (Array.isArray(body.laps) ? body.laps : []) as LapIn[];
    const rows = laps
      .filter((l) => Number.isFinite(Number(l.start_time)))
      .map((l) => ({
        day_id: BigInt(dayId),
        type: "lap",
        start_time: BigInt(Math.round(Number(l.start_time))),
        end_time: l.end_time != null && Number.isFinite(Number(l.end_time)) ? BigInt(Math.round(Number(l.end_time))) : null,
        notes: typeof l.notes === "string" && l.notes.trim() ? l.notes.trim() : null,
      }));

    await prisma.classifier.deleteMany({ where: { day_id: BigInt(dayId), type: "lap" } });
    if (rows.length) {
      await prisma.classifier.createMany({ data: rows, skipDuplicates: true });
    }
    return NextResponse.json({ ok: true, count: rows.length });
  } catch (error) {
    console.error("laps sync error:", error);
    return NextResponse.json({ error: "failed to sync laps" }, { status: 500 });
  }
}
