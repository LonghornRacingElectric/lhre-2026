export const runtime = "nodejs";        // KafkaJS needs Node, not Edge
export const dynamic = "force-dynamic";  // never cache an SSE stream

import { NextRequest } from "next/server";
import { bus } from "@/lib/kafka/bus";
import { ensureSubscribe, startKafkaConsumer } from "@/lib/kafka/kafkaConsumer";
import { getSettings } from "@/lib/motec/config";
import { enrichOrionLiveSample, kafkaTopicFor, kafkaTransportFor, normalizeLivePayload } from "@/lib/motec/live";
import type { LiveStreamEvent } from "@/lib/motec/types";

// The raw nested telemetry feed that normalizeLivePayload understands (dynamics/pack/gps).
const RAW_TELEMETRY_TOPIC = process.env.KAFKA_SENSOR_TOPIC || "sensor_data";

function sampleIntervalMs(sampleHz: number | null): number | null {
  if (sampleHz === null || !Number.isFinite(sampleHz) || sampleHz <= 0) return null;
  return 1000 / sampleHz;
}

/**
 * SSE bridge that adapts the in-app Kafka feed into the trackside dashboard's
 * LiveStreamEvent protocol (status | heartbeat | sample). This is the native
 * replacement for the reference backend's /api/live/stream endpoint: instead of
 * consuming an external Kafka/MQTT broker directly, it reuses our existing
 * Kafka -> bus pipeline (the same one /api/kafka-stream serves) and runs each
 * raw payload through the already-ported normalizeLivePayload().
 */
export async function GET(req: NextRequest) {
  const url = new URL(req.url);
  const source = (url.searchParams.get("source") || "orion").trim().toLowerCase();
  const requestedTopic = url.searchParams.get("topic");
  const transport = kafkaTransportFor(url.searchParams.get("transport"));
  const sampleHzRaw = url.searchParams.get("sampleHz");
  const sampleHz = sampleHzRaw !== null ? Number(sampleHzRaw) : null;
  const minSampleMs = sampleIntervalMs(sampleHz);

  const settings = getSettings(source);
  // The topic label reported back to the UI (mirrors the reference contract).
  const reportedTopic = kafkaTopicFor(source, settings, requestedTopic);

  const enc = new TextEncoder();
  let closed = false;

  const stream = new ReadableStream<Uint8Array>({
    async start(controller) {
      const emit = (event: LiveStreamEvent) => {
        if (closed) return;
        controller.enqueue(enc.encode(`data: ${JSON.stringify(event)}\n\n`));
      };

      // Announce the stream immediately so the UI shows "listening" even before the
      // first sample (and before we touch Kafka, which may be slow to connect).
      emit({
        type: "status",
        ok: true,
        topic: reportedTopic,
        transport,
        message: `Listening to ${reportedTopic}.`,
      });

      // Connect our shared Kafka consumer. If the broker is unreachable, report it
      // as a non-ok status frame and close, rather than hanging the connection.
      try {
        await startKafkaConsumer();
        await ensureSubscribe(RAW_TELEMETRY_TOPIC);
      } catch (e) {
        emit({
          type: "status",
          ok: false,
          topic: reportedTopic,
          transport,
          message: "Live telemetry stream is unavailable.",
          detail: e instanceof Error ? e.message : String(e),
        });
        closed = true;
        try {
          controller.close();
        } catch {
          // already closed
        }
        return;
      }

      let lastSampleEmit = 0;
      let lastActivity = Date.now();

      // Our sensor_data feed is SPARSE: each Kafka message typically carries only
      // ONE populated subsystem table (mostly `dynamics`, with `pack`/`thermal`/
      // `controls` arriving ~1-in-60). To present a complete car state we merge
      // every message's non-empty tables into a running snapshot, then normalize
      // from the merge — so the last-known HV voltage/temps persist between their
      // (infrequent) updates instead of flashing back to zero on dynamics frames.
      const merged: Record<string, unknown> = {};
      const SUBSYSTEM_TABLES = ["dynamics", "controls", "pack", "diagnostics", "thermal"];
      const hasLiveValue = (v: unknown) =>
        v !== null && v !== undefined && v !== 0 && !(Array.isArray(v) && v.length === 0) && v !== "";
      const mergeSnapshot = (decoded: Record<string, unknown>) => {
        for (const [k, v] of Object.entries(decoded)) {
          if (SUBSYSTEM_TABLES.includes(k) && v && typeof v === "object" && !Array.isArray(v)) {
            const prev = (merged[k] as Record<string, unknown> | undefined) ?? {};
            const next: Record<string, unknown> = { ...prev };
            for (const [field, val] of Object.entries(v as Record<string, unknown>)) {
              if (hasLiveValue(val)) next[field] = val; // keep last-known on empty frames
            }
            merged[k] = next;
          } else {
            merged[k] = v; // scalars like time / packetId
          }
        }
      };

      const onMessage = (msg: { topic?: string; payload?: unknown }) => {
        if (closed) return;
        if (msg?.topic !== RAW_TELEMETRY_TOPIC) return;
        const raw = typeof msg.payload === "string" ? msg.payload : JSON.stringify(msg.payload ?? {});

        // Always fold this (possibly sparse) message into the running snapshot,
        // regardless of the sample-rate throttle, so we never drop a rare
        // pack/thermal update just because it arrived inside a throttle window.
        let decoded: Record<string, unknown> | null = null;
        try {
          const obj = JSON.parse(raw) as Record<string, unknown>;
          if (obj && typeof obj === "object") {
            decoded = obj;
            mergeSnapshot(obj);
          }
        } catch {
          // raw wasn't JSON (already-flat payload) — handled below via base only.
        }

        const now = Date.now();
        if (minSampleMs !== null && now - lastSampleEmit < minSampleMs) return;

        const snapshotRaw = decoded ? JSON.stringify(merged) : raw;
        const base = normalizeLivePayload(snapshotRaw, source);
        if (!base) return;
        const sample = decoded ? enrichOrionLiveSample(merged, base) : base;

        lastSampleEmit = now;
        lastActivity = now;
        emit({ type: "sample", topic: reportedTopic, transport, sample });
      };

      bus.on(`kafka:${RAW_TELEMETRY_TOPIC}`, onMessage);

      // Heartbeat keeps the connection alive and lets the UI detect staleness.
      const heartbeat = setInterval(() => {
        if (closed) return;
        if (Date.now() - lastActivity >= 5000) {
          emit({ type: "heartbeat", topic: reportedTopic, transport, t: Date.now() });
        }
      }, 5000);

      const cleanup = () => {
        if (closed) return;
        closed = true;
        clearInterval(heartbeat);
        bus.off(`kafka:${RAW_TELEMETRY_TOPIC}`, onMessage);
        try {
          controller.close();
        } catch {
          // already closed
        }
      };

      req.signal.addEventListener("abort", cleanup);
    },
    cancel() {
      closed = true;
    },
  });

  return new Response(stream, {
    headers: {
      "Content-Type": "text/event-stream; charset=utf-8",
      "Cache-Control": "no-store, no-transform",
      Connection: "keep-alive",
      "X-Accel-Buffering": "no",
    },
  });
}
