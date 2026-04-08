export const runtime = "nodejs";         // require Node (KafkaJS not supported on Edge)
export const dynamic = "force-dynamic";  // don't cache SSE

import { NextRequest } from "next/server";
import { bus } from "@/lib/kafka/bus";
import {
  ensureSubscribe,
  resolveRawTopicsForRequested,
  startKafkaConsumer,
} from "@/lib/kafka/kafkaConsumer";
import { getBufferedMessages } from "@/lib/kafka/messageBuffer";

function normalizeCar(value: string | null): "orion" | "angelique" | null {
  if (!value) return null;
  const v = value.trim().toLowerCase();
  if (v === "orion") return "orion";
  if (v === "angelique") return "angelique";
  return null;
}

function getMessageCarType(msg: any): "orion" | "angelique" | null {
  const rawFromHeader = msg?.headers?.car_type;
  const headerCar = normalizeCar(typeof rawFromHeader === "string" ? rawFromHeader : null);
  if (headerCar) return headerCar;

  if (typeof msg?.payload === "string") {
    try {
      const parsed = JSON.parse(msg.payload);
      const payloadCar =
        normalizeCar(
          typeof parsed?.car_type === "string"
            ? parsed.car_type
            : typeof parsed?.carType === "string"
              ? parsed.carType
              : null,
        );
      if (payloadCar) return payloadCar;
    } catch {
      // ignore
    }
  }
  return null;
}

export async function GET(req: NextRequest) {
  await startKafkaConsumer();

  const url = new URL(req.url);
  const topicsParam = url.searchParams.get("topics");
  const rawTopic = url.searchParams.get("topic");
  const requestedCar = normalizeCar(url.searchParams.get("car"));

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

  const sourceTopics = resolveRawTopicsForRequested(topics);
  await Promise.all(sourceTopics.map((topic) => ensureSubscribe(topic)));

  const enc = new TextEncoder();
  let closed = false;
  const stream = new ReadableStream<Uint8Array>({
    start(controller) {
      const write = (data: unknown) => controller.enqueue(enc.encode(`data: ${JSON.stringify(data)}

`));
      const heartbeat = () => controller.enqueue(enc.encode(`: ping

`));

      // Send buffered history first
      for (const t of topics) {
        const history = getBufferedMessages(t);
        for (const msg of history) {
          const msgCar = getMessageCarType(msg);
          if (requestedCar && msgCar && msgCar !== requestedCar) continue;
          write(msg);
        }
      }

      const handlers: Array<{ topic: string; fn: (msg: any) => void }> = [];
      for (const t of topics) {
        const fn = (msg: any) => {
          if (msg?.topic !== t) return;
          const msgCar = getMessageCarType(msg);
          if (requestedCar && msgCar && msgCar !== requestedCar) return;
          write(msg);
        };
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
      "Connection": "keep-alive",
      "X-Accel-Buffering": "no", // Disable buffering for Nginx/proxies
    },
  });
}
