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

async function buildReplayState(eventId: number, atTimeMs: bigint) {
  const ev = await prisma.event.findUnique({
    where: { event_id: eventId },
    select: { packet_start: true, packet_end: true },
  });

  if (!ev) return { status: 404 as const, body: { error: "Event not found" } };
  if (ev.packet_start == null || ev.packet_end == null) {
    return { status: 409 as const, body: { error: "Event has no packet range" } };
  }

  const packetStart = BigInt(ev.packet_start as any);
  const packetEnd = BigInt(ev.packet_end as any);

  const packet = await prisma.packet.findFirst({
    where: {
      packet_id: { gte: packetStart, lte: packetEnd },
      time: { gte: atTimeMs },
    },
    orderBy: { packet_id: "asc" },
    include: { dynamics: true, controls: true },
  });

  if (!packet) {
    return { status: 404 as const, body: { error: "No packet found at/after time" } };
  }

  const packetId = BigInt(packet.packet_id as any);
  const packetTimeMs = packet.time != null ? BigInt(packet.time as any) : null;

  const nextPacket = await prisma.packet.findFirst({
    where: {
      packet_id: { gt: packetId, lte: packetEnd },
    },
    orderBy: { packet_id: "asc" },
    select: { packet_id: true },
  });

  const isEnd = nextPacket == null || packetId >= packetEnd;

  const d: any = (packet as any).dynamics || {};
  const c: any = (packet as any).controls || {};

  return {
    status: 200 as const,
    body: {
      cursor_ms: bigintToSafeNumber(packetTimeMs),
      packet_id: packetId.toString(),
      time_ms: bigintToSafeNumber(packetTimeMs),
      is_end: isEnd,
      car_visualization: {
        dynamics: {
          flwSpeed: d.flw_speed ?? null,
          frwSpeed: d.frw_speed ?? null,
          blwSpeed: d.blw_speed ?? null,
          brwSpeed: d.brw_speed ?? null,
        },
      },
      map_data: {
        dynamics: {
          gps: d.gps ?? null,
        },
      },
      driver_input_visualizer: {
        controls: {
          steerV: c.steer_v ?? null,
        },
      },
    },
  };
}

function sseHeaders() {
  return {
    "Content-Type": "text/event-stream; charset=utf-8",
    "Cache-Control": "no-cache, no-transform",
    Connection: "keep-alive",
  } as const;
}

function encodeSseEvent(payload: unknown): Uint8Array {
  const data = JSON.stringify(payload);
  return new TextEncoder().encode(`event: state\ndata: ${data}\n\n`);
}

function encodeSseComment(comment: string): Uint8Array {
  return new TextEncoder().encode(`: ${comment}\n\n`);
}

export async function GET(req: NextRequest) {
  try {
    const url = new URL(req.url);
    const eventIdRaw = url.searchParams.get("eventId");
    const startAtMs = toBigInt(url.searchParams.get("startAtTimeMs"));
    const playingRaw = url.searchParams.get("playing");
    const tickMsRaw = url.searchParams.get("tickMs");

    const eventId = eventIdRaw ? Number(eventIdRaw) : NaN;
    if (!Number.isFinite(eventId)) {
      return NextResponse.json({ error: "Missing/invalid 'eventId'" }, { status: 400 });
    }
    if (startAtMs == null) {
      return NextResponse.json({ error: "Missing/invalid 'startAtTimeMs'" }, { status: 400 });
    }

    const playing = playingRaw === "1" || playingRaw === "true";
    const tickMs = (() => {
      const n = tickMsRaw ? Number(tickMsRaw) : 100;
      if (!Number.isFinite(n) || n <= 0) return 100;
      return Math.min(1000, Math.max(25, Math.floor(n)));
    })();

    // If not playing, just send a single snapshot and end.
    if (!playing) {
      const state = await buildReplayState(eventId, startAtMs);
      if (state.status !== 200) {
        return NextResponse.json(state.body, { status: state.status });
      }

      const stream = new ReadableStream<Uint8Array>({
        start(controller) {
          controller.enqueue(encodeSseEvent(state.body));
          controller.close();
        },
      });

      return new NextResponse(stream, { headers: sseHeaders() });
    }

    const wallStart = Date.now();
    const replayStartMs = startAtMs;

    const stream = new ReadableStream<Uint8Array>({
      async start(controller) {
        controller.enqueue(encodeSseComment("connected"));

        let closed = false;
        const stop = () => {
          if (closed) return;
          closed = true;
          try {
            controller.close();
          } catch {
            // ignore
          }
        };

        // Abort when client disconnects.
        const abortSignal: AbortSignal | undefined = (req as any).signal;
        abortSignal?.addEventListener("abort", stop);

        const sendOnce = async () => {
          const elapsed = Date.now() - wallStart;
          const at = replayStartMs + BigInt(Math.max(0, elapsed));

          const state = await buildReplayState(eventId, at);
          if (state.status !== 200) {
            controller.enqueue(encodeSseEvent({ ...state.body, is_end: true }));
            stop();
            return;
          }

          controller.enqueue(encodeSseEvent(state.body));

          if ((state.body as any).is_end) {
            stop();
          }
        };

        // Initial send immediately.
        await sendOnce();
        if (closed) return;

        const interval = setInterval(() => {
          sendOnce().catch(() => {
            // If something fails mid-stream, just terminate.
            stop();
          });
        }, tickMs);

        abortSignal?.addEventListener("abort", () => clearInterval(interval));
      },
      cancel() {
        // no-op; interval cleanup handled by abort listener
      },
    });

    return new NextResponse(stream, { headers: sseHeaders() });
  } catch (e) {
    console.error("Failed to stream replay state", e);
    return NextResponse.json({ error: "Failed to stream replay state" }, { status: 500 });
  }
}
