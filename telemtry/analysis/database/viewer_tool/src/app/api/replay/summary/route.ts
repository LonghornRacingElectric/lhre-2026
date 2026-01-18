import { NextRequest, NextResponse } from "next/server";
import prisma from "@/lib/prisma/telemtry";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

function toNum(v: unknown): number | null {
  if (v == null) return null;
  const n = Number(v);
  return Number.isFinite(n) ? n : null;
}

export async function GET(req: NextRequest) {
  try {
    const url = new URL(req.url);
    const eventIdRaw = url.searchParams.get("eventId");
    const eventId = eventIdRaw ? Number(eventIdRaw) : NaN;
    if (!Number.isFinite(eventId)) {
      return NextResponse.json({ error: "Missing/invalid 'eventId'" }, { status: 400 });
    }

    const event = await prisma.event.findUnique({
      where: { event_id: eventId },
      select: { event_id: true, packet_start: true, packet_end: true },
    });

    if (!event) {
      return NextResponse.json({ error: "Event not found" }, { status: 404 });
    }

    const packetStart = event.packet_start;
    const packetEnd = event.packet_end;

    const [startPacket, endPacket, classifiers] = await Promise.all([
      packetStart != null
        ? prisma.packet.findUnique({ where: { packet_id: packetStart }, select: { time: true } })
        : Promise.resolve(null),
      packetEnd != null
        ? prisma.packet.findUnique({ where: { packet_id: packetEnd }, select: { time: true } })
        : Promise.resolve(null),
      prisma.classifier.findMany({
        where: { event_id: eventId },
        orderBy: [{ start_time: "asc" }, { type: "asc" }],
        select: { type: true, start_time: true, end_time: true, notes: true },
      }),
    ]);

    const timeStart = toNum(startPacket?.time ?? null);
    const timeEnd = toNum(endPacket?.time ?? null);

    const lapTimes = [] as Array<{ start_time: string; end_time: string | null; notes: string | null }>;
    const flaggedEvents = [] as Array<{ type: string; start_time: string; end_time: string | null; notes: string | null }>;

    for (const c of classifiers) {
      const t = (c.type ?? "").toLowerCase();
      const start = c.start_time?.toString?.() ?? String(c.start_time);
      const end = c.end_time != null ? (c.end_time as any).toString?.() ?? String(c.end_time) : null;
      const notes = c.notes ?? null;

      if (t.includes("lap")) {
        lapTimes.push({ start_time: start, end_time: end, notes });
      } else {
        flaggedEvents.push({ type: c.type, start_time: start, end_time: end, notes });
      }
    }

    return NextResponse.json(
      {
        event_id: event.event_id,
        packet_start: packetStart != null ? packetStart.toString() : null,
        packet_end: packetEnd != null ? packetEnd.toString() : null,
        time_start: timeStart,
        time_end: timeEnd,
        lap_times: lapTimes,
        flagged_events: flaggedEvents,
      },
      { status: 200 }
    );
  } catch (e) {
    console.error("Failed to load replay summary", e);
    return NextResponse.json({ error: "Failed to load replay summary" }, { status: 500 });
  }
}
