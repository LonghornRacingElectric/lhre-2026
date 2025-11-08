import { getKafka } from "./kafka";
import { bus, KafkaEvent } from "./bus";

let started = false;
let readyPromise: Promise<void> | null = null;
const subscribedTopics = new Set<string>();
const subscribedRegex = new Set<string>();
let consumer: import("kafkajs").Consumer | null = null;

export async function startKafkaConsumer(): Promise<void> {
  if (started) return readyPromise || Promise.resolve();
  started = true;
  readyPromise = (async () => {
    const kafka = getKafka();
    consumer = kafka.consumer({ groupId: process.env.KAFKA_GROUP_ID || "viewer-tool-group" });
    await consumer.connect();

    const defaultTopics = (process.env.KAFKA_TOPICS || process.env.KAFKA_TOPIC || "")
      .split(",").map(t => t.trim()).filter(Boolean);
    for (const t of defaultTopics) {
      await ensureSubscribe(t);
    }

    await consumer.run({
      eachMessage: async ({ topic, partition, message }) => {
        const payload = message.value ? message.value.toString() : "";
        const headers = Object.fromEntries(
          Object.entries(message.headers || {}).map(([k, v]) => [k, v?.toString()])
        );
        const evt: KafkaEvent = {
          topic,
          partition,
          payload,
          headers,
          offset: message.offset,
          timestamp: message.timestamp,
        };
        bus.emit(`kafka:${topic}` as const, evt);
        bus.emit("kafka:*", evt);
      },
    });

    // graceful-ish
    process.on("beforeExit", async () => {
      try { await consumer?.disconnect(); } catch {}
    });
  })();
  return readyPromise;
}

export async function ensureSubscribe(topic: string | RegExp): Promise<void> {
  if (!topic) return;
  await startKafkaConsumer();
  if (!consumer) return;
  if (typeof topic === "string") {
    if (subscribedTopics.has(topic)) return;
    await consumer.subscribe({ topic, fromBeginning: false });
    subscribedTopics.add(topic);
  } else {
    const key = topic.toString();
    if (subscribedRegex.has(key)) return;
    await consumer.subscribe({ topic, fromBeginning: false });
    subscribedRegex.add(key);
  }
}

export function ensureSubscribePrefix(base: string): Promise<void> {
  const regex = new RegExp(`^${escapeRegex(base)}(?:\\/.*)?$`);
  return ensureSubscribe(regex);
}

function escapeRegex(s: string): string {
  return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}
