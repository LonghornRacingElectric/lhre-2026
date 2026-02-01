import { NextRequest, NextResponse } from "next/server";
import prisma from "@/lib/prisma/telemtry";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

function toBigInt(value: string | null): bigint | null {
  if (value == null || value === "") return null;
  try {
    return BigInt(value);
  } catch {
    return null;
  }
}

function median(values: bigint[]): bigint | null {
  if (!values.length) return null;
  const sorted = [...values].sort((a, b) => (a < b ? -1 : a > b ? 1 : 0));
  return sorted[Math.floor(sorted.length / 2)] ?? null;
}

function bigintToSafeNumber(v: bigint | null): number | null {
  if (v == null) return null;
  const n = Number(v);
  if (!Number.isFinite(n)) return null;
  if (Math.abs(n) > Number.MAX_SAFE_INTEGER) return null;
  return n;
}

export async function GET(req: NextRequest) {
  try {
    const url = new URL(req.url);
    const eventIdRaw = url.searchParams.get("eventId");
    const eventId = eventIdRaw ? Number(eventIdRaw) : NaN;
    if (!Number.isFinite(eventId)) {
      return NextResponse.json(
        { error: "Missing/invalid 'eventId'" },
        { status: 400 },
      );
    }

    const ev = await prisma.event.findUnique({
      where: { event_id: eventId },
      select: {
        event_id: true,
        packet_start: true,
        packet_end: true,
        day_id: true,
        drive_day: { select: { date: true } },
        driver: { select: { driver_name: true } },
        location: { select: { area: true, track: true } },
        eventType: { select: { event_type: true } },
        car: { select: { car_name: true } },
      },
    });
    if (!ev)
      return NextResponse.json({ error: "Event not found" }, { status: 404 });
    if (ev.packet_start == null || ev.packet_end == null) {
      return NextResponse.json(
        { error: "Event has no packet range" },
        { status: 409 },
      );
    }

    const packetStart = BigInt(ev.packet_start as any);
    const packetEnd = BigInt(ev.packet_end as any);

    const [p0, p1] = await Promise.all([
      prisma.packet.findUnique({
        where: { packet_id: packetStart },
        select: { time: true },
      }),
      prisma.packet.findUnique({
        where: { packet_id: packetEnd },
        select: { time: true },
      }),
    ]);

    const startMs = p0?.time != null ? BigInt(p0.time as any) : null;
    const endMs = p1?.time != null ? BigInt(p1.time as any) : null;

    if (startMs == null || endMs == null) {
      return NextResponse.json(
        { error: "Event has no time range" },
        { status: 409 },
      );
    }

    const classifiers = await prisma.classifier.findMany({
      where: {
        AND: [
          { event_id: eventId },
          { start_time: { gte: startMs } },
          {
            OR: [{ end_time: null }, { end_time: { lte: endMs } }],
          },
        ],
      },
      orderBy: [{ start_time: "asc" }],
      select: { type: true, start_time: true, end_time: true, notes: true },
    });

    const lap_times: Array<{
      start_time: string;
      end_time: string | null;
      notes: string | null;
    }> = [];
    const flagged_events: Array<{
      type: string;
      start_time: string;
      end_time: string | null;
      notes: string | null;
    }> = [];

    for (const c of classifiers as any[]) {
      const cStartMs = c.start_time != null ? BigInt(c.start_time as any) : null;
      const cEndMs = c.end_time != null ? BigInt(c.end_time as any) : null;

      if (c.type === "lap") {
        lap_times.push({
          start_time: String(cStartMs),
          end_time: cEndMs != null ? String(cEndMs) : null,
          notes: c.notes ?? null,
        });
      } else {
        flagged_events.push({
          type: String(c.type),
          start_time: String(cStartMs),
          end_time: cEndMs != null ? String(cEndMs) : null,
          notes: c.notes ?? null,
        });
      }
    }

    return NextResponse.json(
      {
        event_id: ev.event_id,
        packet_start: packetStart.toString(),
        packet_end: packetEnd.toString(),
        day_id: ev.day_id,
        day_date: ev.drive_day?.date != null ? ev.drive_day.date.toISOString() : null,
        driver_name: ev.driver?.driver_name ?? null,
        area: ev.location?.area ?? null,
        track: ev.location?.track ?? null,
        event_type: ev.eventType?.event_type ?? null,
        car_name: ev.car?.car_name ?? null,
        time_start: bigintToSafeNumber(startMs),
        time_end: bigintToSafeNumber(endMs),
        lap_times,
        flagged_events,
      },
      { status: 200 },
    );
  } catch (e) {
    console.error("Failed to load replay summary", e);
    return NextResponse.json(
      { error: "Failed to load replay summary" },
      { status: 500 },
    );
  }
}
