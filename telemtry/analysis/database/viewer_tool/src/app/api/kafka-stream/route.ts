export const runtime = "nodejs";         // require Node (KafkaJS not supported on Edge)
export const dynamic = "force-dynamic";  // don't cache SSE

import { NextRequest } from "next/server";
import { bus } from "@/lib/kafka/bus";
import { ensureSubscribe, startKafkaConsumer } from "@/lib/kafka/kafkaConsumer";

export async function GET(req: NextRequest) {
  await startKafkaConsumer();

  const url = new URL(req.url);
  const topic = url.searchParams.get("topic") || process.env.KAFKA_TOPIC || "telemetry";
  await ensureSubscribe(topic);

  const { readable, writable } = new TransformStream();
  const writer = writable.getWriter();
  const enc = new TextEncoder();

  const write = (data: unknown) => writer.write(enc.encode(`data: ${JSON.stringify(data)}\n\n`));
  const heartbeat = () => writer.write(enc.encode(`: ping\n\n`));

  const handler = (msg: any) => {
    if (msg?.topic === topic) write(msg);
  };
  bus.on(`kafka:${topic}`, handler);

  const interval = setInterval(heartbeat, 15000);
  heartbeat();

  req.signal.addEventListener("abort", () => {
    clearInterval(interval);
    bus.off(`kafka:${topic}`, handler);
    writer.close();
  });

  return new Response(readable, {
    headers: {
      "Content-Type": "text/event-stream; charset=utf-8",
      "Cache-Control": "no-store, no-transform",
      Connection: "keep-alive",
    },
  });
}
