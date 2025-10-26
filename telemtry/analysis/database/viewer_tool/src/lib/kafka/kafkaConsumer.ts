import { getKafka } from "./kafka";
import { bus, KafkaEvent } from "./bus";

let started = false;
let readyPromise: Promise<void> | null = null;
let subscribedTopics = new Set<string>();
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

export async function ensureSubscribe(topic: string): Promise<void> {
  if (!topic) return;
  await startKafkaConsumer();
  if (!consumer) return;
  if (subscribedTopics.has(topic)) return;
  await consumer.subscribe({ topic, fromBeginning: false });
  subscribedTopics.add(topic);
}
