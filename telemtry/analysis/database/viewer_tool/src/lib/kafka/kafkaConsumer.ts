import { getKafka } from "./kafka";
import { bus, KafkaEvent } from "./bus";
import type { Kafka, Admin } from "kafkajs";

let started = false;
let readyPromise: Promise<void> | null = null;
const subscribedTopics = new Set<string>();
const subscribedRegex = new Set<string>();
let consumer: import("kafkajs").Consumer | null = null;
let kafkaInstance: Kafka | null = null;
let adminPromise: Promise<Admin> | null = null;

async function getAdmin(): Promise<Admin> {
  if (!kafkaInstance) throw new Error("Kafka instance not initialized yet");
  if (!adminPromise) {
    const admin = kafkaInstance.admin();
    adminPromise = admin.connect().then(() => admin);
  }
  return adminPromise;
}

async function ensureTopicExists(topic: string): Promise<void> {
  // Skip creation for regex or empty
  if (!topic || /[.*+?^${}()|\[\]\\]/.test(topic)) return; // heuristic: treat anything with regex chars as pattern
  try {
    const admin = await getAdmin();
    const existing = await admin.listTopics();
    if (existing.includes(topic)) return;
    await admin.createTopics({
      topics: [{ topic, numPartitions: 1, replicationFactor: 1 }],
      waitForLeaders: true,
    });
    console.log("Created missing topic:", topic);
  } catch (e) {
    console.warn("Topic creation check failed for", topic, e);
  }
}

export async function startKafkaConsumer(): Promise<void> {
  if (started) return readyPromise || Promise.resolve();
  started = true;
  readyPromise = (async () => {
  const kafka = getKafka();
  kafkaInstance = kafka;
  consumer = kafka.consumer({ groupId: process.env.KAFKA_GROUP_ID || "viewer-tool-group" });
    await consumer.connect();

    const defaultTopics = (process.env.KAFKA_TOPICS || process.env.KAFKA_TOPIC || "")
      .split(",").map(t => t.trim()).filter(Boolean);
    console.log("Default topics to subscribe:", defaultTopics);
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
        console.log("Received message on topic:", topic);
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
  if (!consumer) await startKafkaConsumer();
  if (!consumer) return;
  if (typeof topic === "string") {
    if (subscribedTopics.has(topic)) return;
    await ensureTopicExists(topic); // create if missing before subscribing
    try {
      await consumer.subscribe({ topic, fromBeginning: false });
      console.log("Subscribed to topic:", topic);
      subscribedTopics.add(topic);
    } catch (e: any) {
      if (e?.type === 'UNKNOWN_TOPIC_OR_PARTITION') {
        // Retry once after forced creation
        console.warn("Topic unknown, retrying after create:", topic);
        await ensureTopicExists(topic);
        await consumer.subscribe({ topic, fromBeginning: false });
        subscribedTopics.add(topic);
        console.log("Subscribed on retry:", topic);
      } else {
        throw e;
      }
    }
  } else {
    const key = topic.toString();
    if (subscribedRegex.has(key)) return;
    // Pattern subscribe: Kafka will deliver existing matching topics; cannot pre-create reliably.
    await consumer.subscribe({ topic, fromBeginning: false });
    subscribedRegex.add(key);
    console.log("Subscribed (regex):", key);
  }
}

export function ensureSubscribePrefix(base: string): Promise<void> {
  const regex = new RegExp(`^${escapeRegex(base)}(?:\\/.*)?$`);
  return ensureSubscribe(regex);
}

function escapeRegex(s: string): string {
  return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}
