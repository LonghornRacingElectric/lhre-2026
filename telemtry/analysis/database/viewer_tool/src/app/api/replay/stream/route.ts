import { NextRequest, NextResponse } from "next/server";
import prismaTelemtry from "@/lib/prisma/telemtry";
import {
  findNextPacketIdInRange,
  findReplayPacketAtOrAfter,
  normalizeCar,
  SupportedCar,
  resolveCarFromCarId,
} from "@/lib/prisma/carPrisma";

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

function bigintToSafeNumber(v: bigint | null): number | null {
  if (v == null) return null;
  const n = Number(v);
  if (!Number.isFinite(n)) return null;
  if (Math.abs(n) > Number.MAX_SAFE_INTEGER) return null;
  return n;
}

async function buildReplayState(
  eventId: number,
  atTimeMs: bigint,
  preferredCar: SupportedCar | null,
) {
  // eventId maps to day_id since drive_day is the single session record.
  const ev = await prismaTelemtry.drive_day.findUnique({
    where: { day_id: eventId },
    select: {
      packet_start: true,
      packet_end: true,
      car_id: true,
      car: { select: { car_name: true } },
    },
  });

  if (!ev) return { status: 404 as const, body: { error: "Drive day not found" } };
  if (ev.packet_start == null || ev.packet_end == null) {
    return { status: 409 as const, body: { error: "Drive day has no packet range" } };
  }

  const packetStart = BigInt(ev.packet_start as any);
  const packetEnd = BigInt(ev.packet_end as any);
  const car =
    preferredCar ??
    normalizeCar(ev.car?.car_name) ??
    (await resolveCarFromCarId(ev.car_id)) ??
    "orion";
  const packet = await findReplayPacketAtOrAfter(
    car,
    packetStart,
    packetEnd,
    atTimeMs,
  );

  if (!packet) {
    return { status: 404 as const, body: { error: "No packet found at/after time" } };
  }

  const packetId = BigInt(packet.packet_id as any);
  const packetTimeMs = packet.time != null ? BigInt(packet.time as any) : null;

  const nextPacketId = await findNextPacketIdInRange(car, packetId, packetEnd);
  const isEnd = nextPacketId == null || packetId >= packetEnd;

  const d: any = (packet as any).dynamics || {};
  const c: any = (packet as any).controls || {};

  return {
    status: 200 as const,
    body: {
      cursor_ms: bigintToSafeNumber(packetTimeMs),
      packet_id: packetId.toString(),
      time_ms: bigintToSafeNumber(packetTimeMs),
      car_type: car,
      is_end: isEnd,
      car_visualization: {
        dynamics: {
          flwSpeed: d.flw_speed ?? d.flwSpeed ?? null,
          frwSpeed: d.frw_speed ?? d.frwSpeed ?? null,
          blwSpeed: d.blw_speed ?? d.blwSpeed ?? null,
          brwSpeed: d.brw_speed ?? d.brwSpeed ?? null,
        },
      },
      map_data: {
        dynamics: {
          gps: d.gps ?? d.f_gps ?? d.fGps ?? null,
        },
      },
      driver_input_visualizer: {
        controls: {
          steerV: c.steer_v ?? c.steerV ?? null,
          steerColAngle: d.steer_col_angle ?? d.steerColAngle ?? null,
          throttlePct:
            d.accel_pedal_travel ??
            d.accelPedalTravel ??
            c.apps1_t ??
            c.apps1Travel ??
            null,
          brakePct:
            c.brake_pressure_f ??
            c.brakePressureF ??
            c.bse1_v ??
            c.bse1V ??
            null,
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
    const requestedCar = normalizeCar(url.searchParams.get("car"));
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
      const state = await buildReplayState(eventId, startAtMs, requestedCar);
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

          const state = await buildReplayState(eventId, at, requestedCar);
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
