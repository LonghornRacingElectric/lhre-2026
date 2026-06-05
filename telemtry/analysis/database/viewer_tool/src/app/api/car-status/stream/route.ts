export const runtime = "nodejs";        // KafkaJS needs Node, not Edge
export const dynamic = "force-dynamic";  // never cache an SSE stream

import { NextRequest } from "next/server";
import { bus } from "@/lib/kafka/bus";
import { ensureSubscribe, startKafkaConsumer } from "@/lib/kafka/kafkaConsumer";

// The car_status processor publishes high-level state events here.
const CAR_STATUS_TOPIC = process.env.KAFKA_CAR_STATUS_TOPIC || "car_status";

/**
 * SSE bridge that forwards the `car_status` Kafka topic to the browser. It reuses
 * the app's shared Kafka consumer + event bus (same approach as the trackside
 * live bridge) — no new consumer/connection. Each forwarded event is the JSON the
 * processor emitted: { car, kind, state, reasons, hv_soc, lv_v, thresholds, ... }.
 */
export async function GET(req: NextRequest) {
  const url = new URL(req.url);
  const carFilter = (url.searchParams.get("car") || "").trim().toLowerCase();

  const enc = new TextEncoder();
  let closed = false;
  let cleanup = () => {};

  const stream = new ReadableStream<Uint8Array>({
    async start(controller) {
      const send = (data: unknown) => {
        if (closed) return;
        controller.enqueue(enc.encode(`data: ${JSON.stringify(data)}\n\n`));
      };

      try {
        await startKafkaConsumer();
        await ensureSubscribe(CAR_STATUS_TOPIC);
      } catch (e) {
        send({ kind: "status", ok: false, message: "car_status stream unavailable.", detail: String(e) });
        closed = true;
        try { controller.close(); } catch { /* already closed */ }
        return;
      }

      send({ kind: "status", ok: true, message: `Listening on ${CAR_STATUS_TOPIC}.` });

      const onMessage = (msg: { topic?: string; payload?: unknown }) => {
        if (closed || msg?.topic !== CAR_STATUS_TOPIC) return;
        let parsed: Record<string, unknown> | null = null;
        try {
          parsed = typeof msg.payload === "string" ? JSON.parse(msg.payload) : (msg.payload as Record<string, unknown>);
        } catch {
          return;
        }
        if (!parsed) return;
        if (carFilter && String(parsed.car ?? "").toLowerCase() !== carFilter) return;
        send(parsed);
      };

      bus.on(`kafka:${CAR_STATUS_TOPIC}`, onMessage);

      // Keep-alive comment so proxies don't drop an idle connection.
      const keepAlive = setInterval(() => {
        if (closed) return;
        controller.enqueue(enc.encode(`: ping\n\n`));
      }, 15000);

      cleanup = () => {
        if (closed) return;
        closed = true;
        clearInterval(keepAlive);
        bus.off(`kafka:${CAR_STATUS_TOPIC}`, onMessage);
        try { controller.close(); } catch { /* already closed */ }
      };

      req.signal.addEventListener("abort", cleanup);
    },
    cancel() {
      cleanup();
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
