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

// Returns the number of raw time units per millisecond.
async function inferRawUnitsPerMs(packetStart: bigint, packetEnd: bigint): Promise<bigint> {
  const sample = await prisma.packet.findMany({
    where: { packet_id: { gte: packetStart, lte: packetEnd } },
    orderBy: { packet_id: "asc" },
    take: 25,
    select: { time: true },
  });

  const times: bigint[] = sample
    .map((p) => (p.time != null ? BigInt(p.time as any) : null))
    .filter((t): t is bigint => t != null);

  const deltas: bigint[] = [];
  for (let i = 1; i < times.length; i++) {
    const dt = times[i]! - times[i - 1]!;
    if (dt > BigInt(0)) deltas.push(dt);
  }

  const med = median(deltas);
  if (med != null) {
    if (med >= BigInt(1000000)) return BigInt(1000000); // ns -> ms
    if (med >= BigInt(1000)) return BigInt(1000); // us -> ms
    return BigInt(1); // ms
  }

  // Fallback: magnitude-based.
  const t0 = times[0] ?? BigInt(0);
  if (t0 >= BigInt("10000000000000000")) return BigInt(1000000);
  if (t0 >= BigInt("10000000000000")) return BigInt(1000);
  return BigInt(1);
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
    const atTimeRaw = toBigInt(url.searchParams.get("atTime"));
    const atTimeMsRaw = toBigInt(url.searchParams.get("atTimeMs"));

    const eventId = eventIdRaw ? Number(eventIdRaw) : NaN;
    if (!Number.isFinite(eventId)) {
      return NextResponse.json({ error: "Missing/invalid 'eventId'" }, { status: 400 });
    }
    if (atTimeRaw == null && atTimeMsRaw == null) {
      return NextResponse.json(
        { error: "Missing/invalid 'atTimeMs' (or legacy 'atTime')" },
        { status: 400 }
      );
    }

    const ev = await prisma.event.findUnique({
      where: { event_id: eventId },
      select: { packet_start: true, packet_end: true },
    });

    if (!ev) return NextResponse.json({ error: "Event not found" }, { status: 404 });
    if (ev.packet_start == null || ev.packet_end == null) {
      return NextResponse.json({ error: "Event has no packet range" }, { status: 409 });
    }

    const packetStart = BigInt(ev.packet_start as any);
    const packetEnd = BigInt(ev.packet_end as any);

    const rawUnitsPerMs = await inferRawUnitsPerMs(packetStart, packetEnd);
    const atTime =
      atTimeRaw != null
        ? atTimeRaw
        : atTimeMsRaw != null
          ? atTimeMsRaw * rawUnitsPerMs
          : null;

    if (atTime == null) {
      return NextResponse.json({ error: "Missing/invalid 'atTimeMs'" }, { status: 400 });
    }

    const packet = await prisma.packet.findFirst({
      where: {
        packet_id: { gte: packetStart, lte: packetEnd },
        time: { gte: atTime },
      },
      orderBy: { packet_id: "asc" },
      include: { dynamics: true, controls: true },
    });

    if (!packet) {
      return NextResponse.json({ error: "No packet found at/after time" }, { status: 404 });
    }

    const packetId = BigInt(packet.packet_id as any);
    const packetTimeRaw = packet.time != null ? BigInt(packet.time as any) : null;
    const packetTimeMs = packetTimeRaw != null ? packetTimeRaw / rawUnitsPerMs : null;

    const nextPacket = await prisma.packet.findFirst({
      where: {
        packet_id: { gt: packetId, lte: packetEnd },
      },
      orderBy: { packet_id: "asc" },
      select: { time: true },
    });

    const nextTimeRaw = nextPacket?.time != null ? BigInt(nextPacket.time as any) : null;
    const nextTimeMs = nextTimeRaw != null ? nextTimeRaw / rawUnitsPerMs : null;

    const d: any = (packet as any).dynamics || {};
    const c: any = (packet as any).controls || {};

    return NextResponse.json(
      {
        packet_id: packetId.toString(),
        time_raw: packetTimeRaw != null ? packetTimeRaw.toString() : null,
        time_ms: bigintToSafeNumber(packetTimeMs),
        next_time_ms: bigintToSafeNumber(nextTimeMs),
        is_end: nextTimeMs == null || packetId >= packetEnd,
        car_visualization: {
          dynamics: {
            flwSpeed: d.flw_speed ?? null,
            frwSpeed: d.frw_speed ?? null,
            blwSpeed: d.blw_speed ?? null,
            brwSpeed: d.brw_speed ?? null,
          },
        },
        driver_input_visualizer: {
          controls: {
            steerV: c.steer_v ?? null,
          },
        },
      },
      { status: 200 }
    );
  } catch (e) {
    console.error("Failed to load replay state", e);
    return NextResponse.json({ error: "Failed to load replay state" }, { status: 500 });
  }
}
