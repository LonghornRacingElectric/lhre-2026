import { NextRequest, NextResponse } from "next/server";
import { loadOrionBackupReplay } from "@/lib/demo/orionBackupReplay";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

function sseHeaders() {
  return {
    "Content-Type": "text/event-stream; charset=utf-8",
    "Cache-Control": "no-cache, no-transform",
    Connection: "keep-alive",
    "X-Accel-Buffering": "no",
  } as const;
}

function encodeSseEvent(event: string, payload: unknown): Uint8Array {
  return new TextEncoder().encode(`event: ${event}\ndata: ${JSON.stringify(payload)}\n\n`);
}

function clampTickMs(value: string | null): number {
  const parsed = value ? Number(value) : 10;
  if (!Number.isFinite(parsed)) return 10;
  return Math.min(1000, Math.max(10, Math.floor(parsed)));
}

function clampUnit(value: number): number {
  if (!Number.isFinite(value)) return 0;
  return Math.min(1, Math.max(0, value));
}

function lerpNumber(a: number | null | undefined, b: number | null | undefined, alpha: number): number | null {
  if (typeof a !== "number" && typeof b !== "number") return null;
  if (typeof a !== "number") return b ?? null;
  if (typeof b !== "number") return a;
  const t = clampUnit(alpha);
  return a + (b - a) * t;
}

function lerpGps(
  a: number[] | null | undefined,
  b: number[] | null | undefined,
  alpha: number,
): number[] | null {
  if (!a && !b) return null;
  if (!a) return b ?? null;
  if (!b) return a;
  if (a.length < 2 || b.length < 2) return a;
  const t = clampUnit(alpha);
  return [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t];
}

function interpolateFrame(
  a: any,
  b: any,
  replayTimeMs: number,
  alpha: number,
) {
  const gps = lerpGps(a.map?.dynamics?.gps, b.map?.dynamics?.gps, alpha);
  return {
    timestampMs: replayTimeMs,
    dashboard: {
      packetId: typeof a.dashboard?.packetId === "number" ? a.dashboard.packetId : b.dashboard?.packetId ?? null,
      speed: lerpNumber(a.dashboard?.speed, b.dashboard?.speed, alpha),
      wheelSpeedAvg: lerpNumber(a.dashboard?.wheelSpeedAvg, b.dashboard?.wheelSpeedAvg, alpha),
      steerColAngle: lerpNumber(a.dashboard?.steerColAngle, b.dashboard?.steerColAngle, alpha),
      throttlePct: lerpNumber(a.dashboard?.throttlePct, b.dashboard?.throttlePct, alpha),
      brakePct: lerpNumber(a.dashboard?.brakePct, b.dashboard?.brakePct, alpha),
      batteryPct: lerpNumber(a.dashboard?.batteryPct, b.dashboard?.batteryPct, alpha),
      hvPackV: lerpNumber(a.dashboard?.hvPackV, b.dashboard?.hvPackV, alpha),
      hvCurrent: lerpNumber(a.dashboard?.hvCurrent, b.dashboard?.hvCurrent, alpha),
      lvV: lerpNumber(a.dashboard?.lvV, b.dashboard?.lvV, alpha),
      inverterTempC: lerpNumber(a.dashboard?.inverterTempC, b.dashboard?.inverterTempC, alpha),
      motorTempC: lerpNumber(a.dashboard?.motorTempC, b.dashboard?.motorTempC, alpha),
      ambientTempC: lerpNumber(a.dashboard?.ambientTempC, b.dashboard?.ambientTempC, alpha),
    },
    liveBanner: {
      battery: lerpNumber(a.liveBanner?.battery, b.liveBanner?.battery, alpha),
      odometer: lerpNumber(a.liveBanner?.odometer, b.liveBanner?.odometer, alpha),
    },
    energy: {
      powerKw: lerpNumber(a.energy?.powerKw, b.energy?.powerKw, alpha),
      timeSinceOnS: lerpNumber(a.energy?.timeSinceOnS, b.energy?.timeSinceOnS, alpha),
      batteryPct: lerpNumber(a.energy?.batteryPct, b.energy?.batteryPct, alpha),
    },
    map: {
      dynamics: { gps },
    },
    sensor: {
      dynamics: {
        gps,
        fl_sus_pot_v: lerpNumber(a.sensor?.dynamics?.fl_sus_pot_v, b.sensor?.dynamics?.fl_sus_pot_v, alpha),
        fr_sus_pot_v: lerpNumber(a.sensor?.dynamics?.fr_sus_pot_v, b.sensor?.dynamics?.fr_sus_pot_v, alpha),
        bl_sus_pot_v: lerpNumber(a.sensor?.dynamics?.bl_sus_pot_v, b.sensor?.dynamics?.bl_sus_pot_v, alpha),
        br_sus_pot_v: lerpNumber(a.sensor?.dynamics?.br_sus_pot_v, b.sensor?.dynamics?.br_sus_pot_v, alpha),
      },
    },
  };
}

export async function GET(req: NextRequest) {
  try {
    const url = new URL(req.url);
    const tickMs = clampTickMs(url.searchParams.get("tickMs"));
    const loop = url.searchParams.get("loop") !== "0";
    const file = url.searchParams.get("file") ?? undefined;

    const replay = await loadOrionBackupReplay(file);
    const frameCount = replay.frames.length;
    if (!frameCount) {
      return NextResponse.json({ error: "No backup replay frames available" }, { status: 500 });
    }

    const stream = new ReadableStream<Uint8Array>({
      start(controller) {
        controller.enqueue(
          encodeSseEvent("meta", {
            source: replay.fileName,
            sampleMs: replay.sampleMs,
            trimStartMs: replay.trimStartMs,
            trimEndMs: replay.trimEndMs,
            durationMs: replay.durationMs,
            frameCount: replay.frameCount,
            cellTemps: replay.cellTemps,
            outputHz: Math.round(1000 / tickMs),
            loop,
          }),
        );

        const streamStartMs = replay.trimStartMs;
        const durationMs = Math.max(0, replay.durationMs);
        const lastFrameMs = replay.frames[frameCount - 1].timestampMs;
        const wallStartMs = Date.now();

        let scheduledTick = 1;
        let closed = false;
        let cursorIndex = 0;
        let lastReplayTimeMs = Number.NEGATIVE_INFINITY;
        const timerRef: { current: ReturnType<typeof setTimeout> | undefined } = {
          current: undefined,
        };

        const close = () => {
          if (closed) return;
          closed = true;
          if (timerRef.current !== undefined) {
            clearTimeout(timerRef.current);
          }
          try {
            controller.close();
          } catch {
            // no-op
          }
        };

        const abortSignal: AbortSignal | undefined = req.signal;
        abortSignal?.addEventListener("abort", close);

        const emit = () => {
          if (closed) return;

          const elapsedMs = Date.now() - wallStartMs;
          let replayTimeMs = streamStartMs + elapsedMs;
          if (durationMs > 0 && loop) {
            replayTimeMs = streamStartMs + (elapsedMs % durationMs);
          } else if (!loop && durationMs > 0) {
            replayTimeMs = Math.min(streamStartMs + durationMs, replayTimeMs);
          }

          if (replayTimeMs < lastReplayTimeMs) {
            cursorIndex = 0;
          }
          lastReplayTimeMs = replayTimeMs;

          while (
            cursorIndex + 1 < frameCount &&
            replay.frames[cursorIndex + 1].timestampMs <= replayTimeMs
          ) {
            cursorIndex += 1;
          }

          const frameA = replay.frames[cursorIndex];
          const frameB =
            cursorIndex + 1 < frameCount ? replay.frames[cursorIndex + 1] : frameA;
          const spanMs = Math.max(1, frameB.timestampMs - frameA.timestampMs);
          const alpha = frameA === frameB ? 0 : (replayTimeMs - frameA.timestampMs) / spanMs;

          controller.enqueue(
            encodeSseEvent("frame", interpolateFrame(frameA, frameB, replayTimeMs, alpha)),
          );

          if (!loop && replayTimeMs >= lastFrameMs) {
            close();
            return;
          }

          const targetNextMs = wallStartMs + scheduledTick * tickMs;
          scheduledTick += 1;
          const delayMs = Math.max(0, targetNextMs - Date.now());
          timerRef.current = setTimeout(emit, delayMs);
        };

        emit();
      },
      cancel() {
        // closed through abort or natural stream completion
      },
    });

    return new NextResponse(stream, { headers: sseHeaders() });
  } catch (error) {
    console.error("Failed to stream Orion backup replay", error);
    return NextResponse.json({ error: "Failed to stream Orion backup replay" }, { status: 500 });
  }
}
