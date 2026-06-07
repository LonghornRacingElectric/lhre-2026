export const runtime = "nodejs";        // KafkaJS needs Node, not Edge
export const dynamic = "force-dynamic";  // never cache an SSE stream

import { NextRequest } from "next/server";
import { bus } from "@/lib/kafka/bus";
import { ensureSubscribe, startKafkaConsumer } from "@/lib/kafka/kafkaConsumer";
import { getSettings } from "@/lib/motec/config";
import { enrichOrionLiveSample, kafkaTransportFor, normalizeLivePayload } from "@/lib/motec/live";
import type { LiveStreamEvent } from "@/lib/motec/types";

// The raw nested protobuf feed (sparse subsystem tables) — kept as a fallback.
const RAW_TELEMETRY_TOPIC = process.env.KAFKA_SENSOR_TOPIC || "sensor_data";

// The live feed now defaults to the enriched per-car topic produced by the
// field_enricher (grafana_data_<car>_derived): a FLAT, COMPLETE, already-computed
// JSON frame (power_kw, regen, cell deltas, g-forces, …). The raw sensor_data
// path stays available as a reversible fallback — type "sensor_data" in the
// Dash/Live "Topic" (Connection · advanced) field, or set KAFKA_LIVE_TOPIC.
function liveTopicFor(source: string, requested?: string | null): string {
  const r = (requested || "").trim();
  if (r) return r;
  const env = process.env.KAFKA_LIVE_TOPIC?.trim();
  if (env) return env;
  return `grafana_data_${source}_derived`;
}

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

  getSettings(source);
  // The topic we actually consume (enriched per-car by default; sensor_data or a
  // typed topic as fallback). Also the label reported back to the UI.
  const liveTopic = liveTopicFor(source, requestedTopic);
  const isRawProtobuf = liveTopic === RAW_TELEMETRY_TOPIC;
  const reportedTopic = liveTopic;

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
      // Best-effort subscribe. Topics in the consumer's startup list (KAFKA_TOPICS)
      // are already subscribed and this is a no-op; KafkaJS can't add a topic to a
      // running consumer, so a brand-new topic logs a warning here and simply
      // yields no samples (the UI shows "no telemetry") rather than erroring out.
      try {
        await ensureSubscribe(liveTopic);
      } catch (e) {
        console.warn(`live/stream: could not subscribe to ${liveTopic} at runtime; ensure it is in KAFKA_TOPICS`, e);
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
        if (msg?.topic !== liveTopic) return;
        const raw = typeof msg.payload === "string" ? msg.payload : JSON.stringify(msg.payload ?? {});
        const now = Date.now();

        if (isRawProtobuf) {
          // sensor_data: sparse nested frames — fold each (even inside a throttle
          // window) into the running snapshot, then normalize + Orion-enrich.
          let decoded: Record<string, unknown> | null = null;
          try {
            const obj = JSON.parse(raw) as Record<string, unknown>;
            if (obj && typeof obj === "object") {
              decoded = obj;
              mergeSnapshot(obj);
            }
          } catch {
            // raw wasn't JSON (already-flat) — fall through to base-only normalize.
          }
          if (minSampleMs !== null && now - lastSampleEmit < minSampleMs) return;
          const snapshotRaw = decoded ? JSON.stringify(merged) : raw;
          const base = normalizeLivePayload(snapshotRaw, source);
          if (!base) return;
          const sample = decoded ? enrichOrionLiveSample(merged, base) : base;
          lastSampleEmit = now;
          lastActivity = now;
          emit({ type: "sample", topic: reportedTopic, transport, sample });
          return;
        }

        // Enriched grafana_data_<car>_derived: each frame is FLAT, complete and
        // already has the computed channels — no merge, no Orion enrichment.
        if (minSampleMs !== null && now - lastSampleEmit < minSampleMs) return;
        const base = normalizeLivePayload(raw, source);
        if (!base) return;
        lastSampleEmit = now;
        lastActivity = now;
        emit({ type: "sample", topic: reportedTopic, transport, sample: base });
      };

      bus.on(`kafka:${liveTopic}`, onMessage);

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
        bus.off(`kafka:${liveTopic}`, onMessage);
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
