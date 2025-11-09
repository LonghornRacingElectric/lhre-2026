export const runtime = "nodejs";         // require Node (KafkaJS not supported on Edge)
export const dynamic = "force-dynamic";  // don't cache SSE

import { NextRequest } from "next/server";
import { bus } from "@/lib/kafka/bus";
import { startKafkaConsumer } from "@/lib/kafka/kafkaConsumer";

export async function GET(req: NextRequest) {
  await startKafkaConsumer();

  const url = new URL(req.url);
  const topicsParam = url.searchParams.get("topics");
  const rawTopic = url.searchParams.get("topic");

  const topics: string[] = (topicsParam
    ? topicsParam.split(",")
    : rawTopic
      ? [rawTopic]
      : [])
    .map(t => t.trim())
    .filter(Boolean);

  if (!topics.length) {
    return new Response("Missing 'topics' or 'topic' query parameter", { status: 400 });
  }

  // Do not subscribe to Kafka here. The consumer subscribes to raw topics from env (KAFKA_TOPICS).
  // We only attach bus listeners for the exact topics requested.

  const enc = new TextEncoder();
  let closed = false;
  const stream = new ReadableStream<Uint8Array>({
    start(controller) {
      const write = (data: unknown) => controller.enqueue(enc.encode(`data: ${JSON.stringify(data)}\n\n`));
      const heartbeat = () => controller.enqueue(enc.encode(`: ping\n\n`));

      const handlers: Array<{ topic: string; fn: (msg: any) => void }> = [];
      for (const t of topics) {
        const fn = (msg: any) => { if (msg?.topic === t) write(msg); };
        bus.on(`kafka:${t}`, fn);
        handlers.push({ topic: t, fn });
      }

      const interval = setInterval(heartbeat, 15000);
      heartbeat();

      const cleanup = () => {
        if (closed) return;
        closed = true;
        clearInterval(interval);
        for (const h of handlers) bus.off(`kafka:${h.topic}`, h.fn);
        try { controller.close(); } catch {}
      };

      req.signal.addEventListener("abort", cleanup);
      // Defensive: also close if page hides (optional) – omitted for now
    },
    cancel() {
      closed = true;
    }
  });

  return new Response(stream, {
    headers: {
      "Content-Type": "text/event-stream; charset=utf-8",
      "Cache-Control": "no-store, no-transform",
      Connection: "keep-alive",
    },
  });
}
