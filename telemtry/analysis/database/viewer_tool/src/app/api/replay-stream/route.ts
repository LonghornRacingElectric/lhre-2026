export const runtime = 'nodejs';
export const dynamic = 'force-dynamic';

import { NextRequest } from 'next/server';
import prisma from '@/lib/prisma/telemtry';

type KafkaEvent = {
  topic: string;
  partition: number;
  payload: string;
  headers?: Record<string, string | undefined>;
  offset: string;
  timestamp: string;
};

function toBigInt(value: string | null): bigint | null {
  if (value == null || value === '') return null;
  try {
    return BigInt(value);
  } catch {
    return null;
  }
}

function sleep(ms: number): Promise<void> {
  return new Promise((r) => setTimeout(r, ms));
}

export async function GET(req: NextRequest) {
  const url = new URL(req.url);
  const topicsParam = url.searchParams.get('topics');
  const eventId = url.searchParams.get('eventId');
  const fromTime = toBigInt(url.searchParams.get('fromTime'));
  const speed = Math.max(0.1, Math.min(10, Number(url.searchParams.get('speed') ?? '1')));
  const paused = url.searchParams.get('paused') === '1' || url.searchParams.get('paused') === 'true';

  const topics: string[] = (topicsParam ? topicsParam.split(',') : [])
    .map((t) => t.trim())
    .filter(Boolean);

  if (!eventId) {
    return new Response("Missing 'eventId' query parameter", { status: 400 });
  }
  if (!topics.length) {
    return new Response("Missing 'topics' query parameter", { status: 400 });
  }

  // Resolve packet range for the event
  const ev = await prisma.event.findUnique({
    where: { event_id: Number(eventId) },
    select: { event_id: true, packet_start: true, packet_end: true },
  });
  if (!ev) return new Response('Event not found', { status: 404 });

  const packetStart: bigint | null = ev.packet_start != null ? BigInt(ev.packet_start as any) : null;
  const packetEnd: bigint | null = ev.packet_end != null ? BigInt(ev.packet_end as any) : null;

  if (packetStart == null || packetEnd == null) {
    return new Response('Event has no packet_start/packet_end', { status: 409 });
  }

  let startId: bigint = packetStart;
  if (fromTime != null) {
    const p = await prisma.packet.findFirst({
      where: {
        packet_id: { gte: packetStart, lte: packetEnd },
        time: { gte: fromTime },
      },
      orderBy: { packet_id: 'asc' },
      select: { packet_id: true },
    });
    if (p?.packet_id != null) startId = BigInt(p.packet_id as any);
  }

  const enc = new TextEncoder();
  let closed = false;
  let cleanupRef: (() => void) | null = null;

  const stream = new ReadableStream<Uint8Array>({
    start(controller) {
      let hb: ReturnType<typeof setInterval> | null = null;

      const safeEnqueue = (chunk: Uint8Array) => {
        if (closed) return;
        try {
          controller.enqueue(chunk);
        } catch {
          closed = true;
        }
      };

      const write = (data: unknown) =>
        safeEnqueue(enc.encode(`data: ${JSON.stringify(data)}\n\n`));
      const heartbeat = () => safeEnqueue(enc.encode(`: ping\n\n`));

      const cleanup = () => {
        if (closed) return;
        closed = true;
        if (hb) {
          clearInterval(hb);
          hb = null;
        }
        req.signal.removeEventListener('abort', onAbort);
        try {
          controller.close();
        } catch {}
      };

      const onAbort = () => cleanup();
      req.signal.addEventListener('abort', onAbort);
      cleanupRef = cleanup;

      // Keepalive
      hb = setInterval(heartbeat, 15000);
      heartbeat();

      (async () => {
        try {
          if (paused) {
            // Emit a single snapshot at the requested cursor, then keepalive.
            const snap = await prisma.packet.findUnique({
              where: { packet_id: startId },
              include: { dynamics: true, pack: true, controls: true, diagnostics: true },
            });

            if (snap) {
              const packetId = BigInt(snap.packet_id as any);
              const t = snap.time != null ? BigInt(snap.time as any) : null;
              const base: Omit<KafkaEvent, 'topic' | 'payload'> = {
                partition: 0,
                headers: {},
                offset: String(packetId),
                timestamp: t != null ? t.toString() : Date.now().toString(),
              };

              for (const topic of topics) {
                let payloadObj: any | undefined;
                if (topic === 'car_visualization') {
                  const d: any = (snap as any).dynamics || {};
                  payloadObj = {
                    dynamics: {
                      flwSpeed: d.flw_speed ?? null,
                      frwSpeed: d.frw_speed ?? null,
                      blwSpeed: d.blw_speed ?? null,
                      brwSpeed: d.brw_speed ?? null,
                      inverterV: d.inverter_v ?? null,
                    },
                  };
                } else if (topic === 'live_banner') {
                  const diag: any = (snap as any).diagnostics || {};
                  payloadObj = {
                    battery: diag.hv_charge_state ?? null,
                    odometer: diag.odometer ?? null,
                  };
                } else if (topic === 'driver_input_visualizer') {
                  const c: any = (snap as any).controls || {};
                  payloadObj = { controls: { steerV: c.steer_v ?? null } };
                } else if (topic === 'gg-plot') {
                  const d: any = (snap as any).dynamics || {};
                  const a = d.vcu_accel ?? d.body1_accel ?? null;
                  const x = Array.isArray(a) ? a[0] : null;
                  const y = Array.isArray(a) ? a[1] : null;
                  payloadObj = { data: { x, y } };
                } else {
                  continue;
                }

                write({
                  ...base,
                  topic,
                  payload: JSON.stringify(payloadObj),
                } satisfies KafkaEvent);
              }
            }

            while (!closed) await sleep(1000);
            return;
          }

          // Fetch packets in chunks to avoid huge memory usage
          const chunkSize = 500;
          let currentId = startId;
          let lastTime: bigint | null = null;

          while (!closed && currentId <= packetEnd) {
            const rows = await prisma.packet.findMany({
              where: { packet_id: { gte: currentId, lte: packetEnd } },
              orderBy: { packet_id: 'asc' },
              take: chunkSize,
              include: { dynamics: true, pack: true, controls: true, diagnostics: true },
            });

            if (rows.length === 0) break;

            for (const row of rows as any[]) {
              if (closed) break;

              const packetId = BigInt(row.packet_id as any);
              const t = row.time != null ? BigInt(row.time as any) : null;

              if (lastTime != null && t != null) {
                const dtMs = Number(t - lastTime);
                // Clamp delays so the UI stays responsive even if there are gaps
                const waitMs = Math.max(0, Math.min(1000, Math.floor(dtMs / speed)));
                if (waitMs > 0) await sleep(waitMs);
              }
              lastTime = t;

              const base: Omit<KafkaEvent, 'topic' | 'payload'> = {
                partition: 0,
                headers: {},
                offset: String(packetId),
                timestamp: t != null ? t.toString() : Date.now().toString(),
              };

              // Build per-topic payloads from DB rows.
              for (const topic of topics) {
                let payloadObj: any | undefined;

                if (topic === 'car_visualization') {
                  const d = row.dynamics || {};
                  payloadObj = {
                    dynamics: {
                      flwSpeed: d.flw_speed ?? null,
                      frwSpeed: d.frw_speed ?? null,
                      blwSpeed: d.blw_speed ?? null,
                      brwSpeed: d.brw_speed ?? null,
                      inverterV: d.inverter_v ?? null,
                    },
                  };
                } else if (topic === 'live_banner') {
                  const diag = row.diagnostics || {};
                  payloadObj = {
                    battery: diag.hv_charge_state ?? null,
                    odometer: diag.odometer ?? null,
                  };
                } else if (topic === 'driver_input_visualizer') {
                  const c = row.controls || {};
                  payloadObj = { controls: { steerV: c.steer_v ?? null } };
                } else if (topic === 'gg-plot') {
                  const d = row.dynamics || {};
                  const a = d.vcu_accel ?? d.body1_accel ?? null;
                  const x = Array.isArray(a) ? a[0] : null;
                  const y = Array.isArray(a) ? a[1] : null;
                  payloadObj = { data: { x, y } };
                } else {
                  // Unknown topic for replay: skip emitting to avoid spamming.
                  continue;
                }

                const evt: KafkaEvent = {
                  ...base,
                  topic,
                  payload: JSON.stringify(payloadObj),
                };
                write(evt);
              }

              currentId = packetId + BigInt(1);
            }

            // If we ended early due to gaps, advance to next expected packet id
            const lastRow: any = rows[rows.length - 1];
            currentId = BigInt(lastRow.packet_id as any) + BigInt(1);
          }

          // End of stream: send empty objects to allow merge-enabled consumers to reset if desired.
          for (const topic of topics) {
            const evt: KafkaEvent = {
              partition: 0,
              headers: {},
              offset: packetEnd.toString(),
              timestamp: Date.now().toString(),
              topic,
              payload: JSON.stringify({}),
            };
            write(evt);
          }
        } catch (e) {
          console.error('Replay stream error', e);
        } finally {
          cleanup();
        }
      })();
    },
    cancel() {
      cleanupRef?.();
    },
  });

  return new Response(stream, {
    headers: {
      'Content-Type': 'text/event-stream; charset=utf-8',
      'Cache-Control': 'no-store, no-transform',
      Connection: 'keep-alive',
      'X-Accel-Buffering': 'no',
    },
  });
}
