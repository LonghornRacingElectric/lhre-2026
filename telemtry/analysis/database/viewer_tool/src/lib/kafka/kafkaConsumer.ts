import { getKafka } from "./kafka";
import { bus, KafkaEvent } from "./bus";
import fs from "fs";
import path from "path";

// Mapping rules: raw Kafka topic -> array of outbound logical component topics with transformers.
// Configure via process.env.KAFKA_ROUTES_JSON (JSON string) or build defaults here.
// Example JSON:
// {
//   "raw-status": [{ "to": "3d-visualizer", "pick": ["battery", "odometer"] },
//                   { "to": "gg-plot", "pick": ["battery"] }],
//   "raw-telemetry": [{ "to": "gg-plot", "pick": ["rpm","speed"] }]
// }
type RouteRule = { to: string; pick?: string[]; rename?: Record<string,string>; }; // simple transformation config
let routeConfig: Record<string, RouteRule[]> = {};

function loadRouteConfig() {
  if (routeConfig && Object.keys(routeConfig).length) return routeConfig;
  // 1) Prefer file path via KAFKA_ROUTES_FILE
  const fileEnv = process.env.KAFKA_ROUTES_FILE;
  if (fileEnv) {
    try {
      const filePath = path.isAbsolute(fileEnv) ? fileEnv : path.join(process.cwd(), fileEnv);
      const txt = fs.readFileSync(filePath, "utf8");
      routeConfig = JSON.parse(txt);
      console.log("Loaded route config from KAFKA_ROUTES_FILE:", filePath);
      return routeConfig;
    } catch (e) {
      console.warn("Failed to load KAFKA_ROUTES_FILE, falling back to env or default", e);
    }
  }
  // 2) Backward-compat: support JSON in env var
  try {
    const json = process.env.KAFKA_ROUTES_JSON;
    if (json) {
      routeConfig = JSON.parse(json);
      console.log("Loaded KAFKA_ROUTES_JSON routeConfig");
      return routeConfig;
    }
  } catch (e) {
    console.warn("Failed parsing KAFKA_ROUTES_JSON", e);
  }
  // 3) Try default file kafka.routes.json at project root
  try {
    const defaultPath = path.join(process.cwd(), "kafka.routes.json");
    if (fs.existsSync(defaultPath)) {
      const txt = fs.readFileSync(defaultPath, "utf8");
      routeConfig = JSON.parse(txt);
      console.log("Loaded route config from default kafka.routes.json");
    }
  } catch (e) {
    console.warn("No routing config found (this is OK if you only consume raw topics)");
  }
  return routeConfig;
}
import type { Kafka, Admin } from "kafkajs";
import { AngeliqueSensorData } from "@protobuf/angelique";

let started = false;
let readyPromise: Promise<void> | null = null;
const subscribedTopics = new Set<string>();
const subscribedRegex = new Set<string>();
let consumer: import("kafkajs").Consumer | null = null;
let kafkaInstance: Kafka | null = null;
let adminPromise: Promise<Admin> | null = null;

// Helpers to support dot-path picks like "dynamics.speed"
function getByPath(obj: any, path: string | string[]): any {
  const parts = Array.isArray(path) ? path : path.split(".").filter(Boolean);
  let cur = obj;
  for (const p of parts) {
    if (cur == null || typeof cur !== "object" || !(p in cur)) return undefined;
    cur = (cur as any)[p];
  }
  return cur;
}

function setByPath(target: any, path: string | string[], value: any): void {
  const parts = Array.isArray(path) ? path : path.split(".").filter(Boolean);
  if (!parts.length) return;
  let cur = target;
  for (let i = 0; i < parts.length - 1; i++) {
    const p = parts[i];
    if (cur[p] == null || typeof cur[p] !== "object" || Array.isArray(cur[p])) {
      cur[p] = {};
    }
    cur = cur[p];
  }
  cur[parts[parts.length - 1]] = value;
}

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
    consumer = kafka.consumer({
      groupId: process.env.KAFKA_GROUP_ID || "viewer-tool-group",
    });
    await consumer.connect();

    const defaultTopics = (
      process.env.KAFKA_TOPICS ||
      process.env.KAFKA_TOPIC ||
      ""
    )
      .split(",")
      .map((t) => t.trim())
      .filter(Boolean);
    console.log("Default topics to subscribe:", defaultTopics);
    for (const t of defaultTopics) {
      await ensureSubscribe(t);
    }

    // Ensure the Kafka consumer processes messages as soon as they arrive
    await consumer.run({
      eachMessage: async ({ topic, partition, message }) => {
        // Added logging to track message processing time
        // console.log(`Processing message from topic ${topic} at ${new Date().toISOString()}`);

        let payload = message.value ? message.value.toString() : "";
        let parsed: any = null;
        
        const headers = Object.fromEntries(
          Object.entries(message.headers || {}).map(([k, v]) => [
            k,
            v?.toString(),
          ])
        );

        // Try to decode protobuf first, fall back to JSON/string
        try {
          if (message.value) {
            const decodedPayload = AngeliqueSensorData.decode(
              new Uint8Array(message.value)
            );
            parsed = AngeliqueSensorData.toJSON(decodedPayload);
            payload = JSON.stringify(parsed);
            // console.log("Decoded AngeliqueSensorData payload for topic:", topic);
          }
        } catch (error) {
          console.log("Failed to decode as protobuf, trying JSON for topic:", topic);
          // If protobuf decoding fails, try JSON
          try {
            parsed = JSON.parse(payload);
          } catch {
            parsed = payload; // Keep as string if JSON parse also fails
          }
        }

        const evt: KafkaEvent = {
          topic,
          partition,
          payload,
          headers,
          offset: message.offset,
          timestamp: message.timestamp,
        };

        // console.log("Received message on topic:", topic);
        bus.emit(`kafka:${topic}` as const, evt); // raw event
        bus.emit("kafka:*", evt);

        // Routing layer: transform and emit logical component topics
        const routes = loadRouteConfig()[topic];
        if (routes && routes.length) {
          // Use the already parsed data instead of re-parsing
          for (const rule of routes) {
            let outData = parsed;
            if (parsed && typeof parsed === "object" && !Array.isArray(parsed)) {
              if (rule.pick && rule.pick.length) {
                const subset: Record<string, any> = {};
                for (const key of rule.pick) {
                  const val = getByPath(parsed, key);
                  if (val !== undefined) {
                    // If rename provided, allow renaming of the leaf property name
                    const parts = key.split(".").filter(Boolean);
                    if (rule.rename && parts.length) {
                      const leaf = parts[parts.length - 1];
                      const renamedLeaf = rule.rename[leaf];
                      if (renamedLeaf) parts[parts.length - 1] = renamedLeaf;
                    }
                    setByPath(subset, parts, val);
                  }
                }
                outData = subset;
              } else if (rule.rename && Object.keys(rule.rename).length) {
                // Back-compat: if only rename is provided, apply to top-level keys
                const renamed: Record<string, any> = {};
                for (const [k, v] of Object.entries(parsed)) {
                  const newKey = rule.rename![k] || k;
                  renamed[newKey] = v;
                }
                outData = renamed;
              }
            }
            const routed: KafkaEvent = {
              topic: rule.to,
              partition,
              payload: typeof outData === 'string' ? outData : JSON.stringify(outData),
              headers,
              offset: message.offset,
              timestamp: message.timestamp,
            };
            bus.emit(`kafka:${rule.to}` as const, routed);
            console.log(`Routed message from topic ${topic} to ${rule.to} with following data: `, routed);

          }
        }
      },
    });

    // graceful-ish
    process.on("beforeExit", async () => {
      try {
        await consumer?.disconnect();
      } catch {}
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
      if (e?.type === "UNKNOWN_TOPIC_OR_PARTITION") {
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
