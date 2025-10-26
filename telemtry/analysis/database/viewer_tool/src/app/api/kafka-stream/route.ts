export const runtime = "nodejs";         // require Node (KafkaJS not supported on Edge)
export const dynamic = "force-dynamic";  // don't cache SSE

import { NextRequest } from "next/server";
import { bus } from "@/lib/kafka/bus";
import { ensureSubscribe, ensureSubscribePrefix, startKafkaConsumer } from "@/lib/kafka/kafkaConsumer";

export async function GET(req: NextRequest) {
  await startKafkaConsumer();

  const url = new URL(req.url);
  const rawTopic = url.searchParams.get("topic");
  if(!rawTopic) return new Response("Missing 'topic' query parameter", { status: 400 });
  const prefixParam = url.searchParams.get("prefix");
  const isPrefix = rawTopic.endsWith("/*") || (prefixParam?.toLowerCase() === "1" || prefixParam?.toLowerCase() === "true");
  const base = rawTopic.replace(/\/\*$/, "");

  if (isPrefix) {
    await ensureSubscribePrefix(base);
  } else {
    await ensureSubscribe(base);
  }

  const { readable, writable } = new TransformStream();
  const writer = writable.getWriter();
  const enc = new TextEncoder();

  const write = (data: unknown) => writer.write(enc.encode(`data: ${JSON.stringify(data)}\n\n`));
  const heartbeat = () => writer.write(enc.encode(`: ping\n\n`));

  const handler = (msg: any) => {
    if (!msg?.topic) return;
    if (isPrefix) {
      if (msg.topic === base || msg.topic.startsWith(base + "/")) write(msg);
    } else {
      if (msg.topic === base) write(msg);
    }
  };
  if (isPrefix) {
    bus.on("kafka:*", handler);
  } else {
    bus.on(`kafka:${base}`, handler);
  }

  const interval = setInterval(heartbeat, 15000);
  heartbeat();

  req.signal.addEventListener("abort", () => {
    clearInterval(interval);
    if (isPrefix) {
      bus.off("kafka:*", handler);
    } else {
      bus.off(`kafka:${base}`, handler);
    }
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
