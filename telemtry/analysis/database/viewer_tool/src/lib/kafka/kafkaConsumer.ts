import fs from "fs";
import path from "path";
import protobuf from "protobufjs";

import type { Admin, Kafka } from "kafkajs";
import { AngeliqueSensorData } from "@protobuf/angelique";

import { bus, KafkaEvent } from "./bus";
import { getKafka } from "./kafka";
import { bufferMessage } from "./messageBuffer";

type RouteRule = {
  to: string;
  pick?: string[];
  rename?: Record<string, string>;
};

type ParsedRecord = Record<string, unknown>;

const SENSOR_DATA_TOPIC = process.env.KAFKA_SENSOR_TOPIC || "sensor_data";
const DEFAULT_MAX_MESSAGE_AGE_MS = 5000;
const configuredMaxAgeMs = Number(process.env.KAFKA_MAX_MESSAGE_AGE_MS ?? DEFAULT_MAX_MESSAGE_AGE_MS);
const MAX_MESSAGE_AGE_MS =
  Number.isFinite(configuredMaxAgeMs) && configuredMaxAgeMs > 0
    ? configuredMaxAgeMs
    : 0;
const NORMALIZED_SENSOR_TOPICS = new Set([
  "car_visualization",
  "driver_input_visualizer",
  "map",
  "live_banner",
  "dashboard_screen",
  "timing_deltas",
  "shutdown_screen",
  "thermal_headroom",
  "energy_budget",
]);

let routeConfig: Record<string, RouteRule[]> = {};
let started = false;
let readyPromise: Promise<void> | null = null;
const subscribedTopics = new Set<string>();
const subscribedRegex = new Set<string>();
let consumer: import("kafkajs").Consumer | null = null;
let kafkaInstance: Kafka | null = null;
let adminPromise: Promise<Admin> | null = null;
let orionTypePromise: Promise<protobuf.Type | null> | null = null;

function normalizeRouteConfig(raw: unknown): Record<string, RouteRule[]> {
  if (!raw || typeof raw !== "object") return {};
  const normalized: Record<string, RouteRule[]> = {};

  for (const [topic, value] of Object.entries(raw as Record<string, unknown>)) {
    const rules = Array.isArray(value) ? value : value ? [value] : [];
    const validRules = rules
      .filter((rule): rule is Record<string, unknown> => !!rule && typeof rule === "object")
      .map((rule): RouteRule | null => {
        const to = typeof rule.to === "string" ? rule.to.trim() : "";
        if (!to) return null;
        const pick = Array.isArray(rule.pick)
          ? rule.pick.filter((p): p is string => typeof p === "string" && !!p.trim())
          : undefined;
        const rename =
          rule.rename && typeof rule.rename === "object"
            ? Object.fromEntries(
                Object.entries(rule.rename as Record<string, unknown>).filter(
                  ([k, v]) => typeof k === "string" && typeof v === "string",
                ) as Array<[string, string]>,
              )
            : undefined;
        return { to, pick, rename };
      })
      .filter((rule): rule is RouteRule => rule !== null);

    if (validRules.length) {
      normalized[topic] = validRules;
    }
  }

  return normalized;
}

function loadRouteConfig() {
  if (Object.keys(routeConfig).length) return routeConfig;

  const fileEnv = process.env.KAFKA_ROUTES_FILE;
  if (fileEnv) {
    try {
      const filePath = path.isAbsolute(fileEnv)
        ? fileEnv
        : path.join(process.cwd(), fileEnv);
      const txt = fs.readFileSync(filePath, "utf8");
      routeConfig = normalizeRouteConfig(JSON.parse(txt));
      console.log("Loaded route config from KAFKA_ROUTES_FILE:", filePath);
      return routeConfig;
    } catch (e) {
      console.warn("Failed to load KAFKA_ROUTES_FILE, falling back to env/default", e);
    }
  }

  try {
    const json = process.env.KAFKA_ROUTES_JSON;
    if (json) {
      routeConfig = normalizeRouteConfig(JSON.parse(json));
      console.log("Loaded KAFKA_ROUTES_JSON routeConfig");
      return routeConfig;
    }
  } catch (e) {
    console.warn("Failed parsing KAFKA_ROUTES_JSON", e);
  }

  try {
    const defaultPath = path.join(process.cwd(), "kafka.routes.json");
    if (fs.existsSync(defaultPath)) {
      const txt = fs.readFileSync(defaultPath, "utf8");
      routeConfig = normalizeRouteConfig(JSON.parse(txt));
      console.log("Loaded route config from default kafka.routes.json");
    }
  } catch (e) {
    console.warn("No routing config found (acceptable for raw-topic only mode)", e);
  }

  return routeConfig;
}

function getByPath(obj: unknown, pathOrParts: string | string[]): unknown {
  const parts = Array.isArray(pathOrParts)
    ? pathOrParts
    : pathOrParts.split(".").filter(Boolean);
  let cur: any = obj;
  for (const p of parts) {
    if (cur == null || typeof cur !== "object" || !(p in cur)) return undefined;
    cur = cur[p];
  }
  return cur;
}

function setByPath(target: Record<string, unknown>, pathOrParts: string | string[], value: unknown): void {
  const parts = Array.isArray(pathOrParts)
    ? pathOrParts
    : pathOrParts.split(".").filter(Boolean);
  if (!parts.length) return;

  let cur: any = target;
  for (let i = 0; i < parts.length - 1; i++) {
    const p = parts[i];
    if (cur[p] == null || typeof cur[p] !== "object" || Array.isArray(cur[p])) {
      cur[p] = {};
    }
    cur = cur[p];
  }
  cur[parts[parts.length - 1]] = value;
}

function toCamelCase(input: string): string {
  return input.replace(/_([a-z0-9])/g, (_, c: string) => c.toUpperCase());
}

function camelizeKeysDeep<T>(value: T): T {
  if (Array.isArray(value)) {
    return value.map((entry) => camelizeKeysDeep(entry)) as T;
  }
  if (value && typeof value === "object") {
    const out: Record<string, unknown> = {};
    for (const [k, v] of Object.entries(value as Record<string, unknown>)) {
      out[toCamelCase(k)] = camelizeKeysDeep(v);
    }
    return out as T;
  }
  return value;
}

function normalizeCarType(rawCarType: unknown): "angelique" | "orion" | "nightwatch" | null {
  if (typeof rawCarType !== "string") return null;
  const v = rawCarType.trim().toLowerCase();
  if (v === "angelique") return "angelique";
  if (v === "orion") return "orion";
  if (v === "nightwatch") return "nightwatch";
  return null;
}

function parsePossiblyJson(payload: string): unknown {
  try {
    return JSON.parse(payload);
  } catch {
    return payload;
  }
}

async function getOrionMessageType(): Promise<protobuf.Type | null> {
  if (!orionTypePromise) {
    orionTypePromise = (async () => {
      try {
        const protoPath = path.join(process.cwd(), "protobuf", "orion.proto");
        const root = await protobuf.load(protoPath);
        return root.lookupType("orion.OrionSensorData");
      } catch (e) {
        console.error("Failed to load Orion protobuf schema", e);
        return null;
      }
    })();
  }
  return orionTypePromise;
}

function tryDecodeAngelique(value: Buffer): ParsedRecord | null {
  try {
    const decoded = AngeliqueSensorData.decode(new Uint8Array(value));
    return AngeliqueSensorData.toJSON(decoded) as ParsedRecord;
  } catch {
    return null;
  }
}

async function tryDecodeOrion(value: Buffer): Promise<ParsedRecord | null> {
  const orionType = await getOrionMessageType();
  if (!orionType) return null;

  try {
    const decoded = orionType.decode(new Uint8Array(value));
    const asObject = orionType.toObject(decoded, {
      longs: Number,
      enums: String,
      defaults: true,
      arrays: true,
      objects: true,
    });
    return camelizeKeysDeep(asObject) as ParsedRecord;
  } catch {
    return null;
  }
}

async function decodeSensorMessageByCar(
  value: Buffer,
  preferredCar: "angelique" | "orion" | "nightwatch" | null,
): Promise<ParsedRecord | null> {
  if (preferredCar === "angelique") {
    return tryDecodeAngelique(value) ?? (await tryDecodeOrion(value));
  }
  if (preferredCar === "orion") {
    return (await tryDecodeOrion(value)) ?? tryDecodeAngelique(value);
  }
  return tryDecodeAngelique(value) ?? (await tryDecodeOrion(value));
}

function firstDefined(obj: unknown, paths: string[]): unknown {
  for (const p of paths) {
    const value = getByPath(obj, p);
    if (value !== undefined && value !== null) return value;
  }
  return undefined;
}

function toNumber(value: unknown): number | undefined {
  const n = typeof value === "number" ? value : Number(value);
  return Number.isFinite(n) ? n : undefined;
}

function toNumberPair(value: unknown): [number, number] | undefined {
  if (!Array.isArray(value) || value.length < 2) return undefined;
  const x = toNumber(value[0]);
  const y = toNumber(value[1]);
  if (x === undefined || y === undefined) return undefined;
  return [x, y];
}

function toBoolean(value: unknown): boolean | undefined {
  if (typeof value === "boolean") return value;
  if (typeof value === "number") {
    if (value === 1) return true;
    if (value === 0) return false;
  }
  if (typeof value === "string") {
    const normalized = value.trim().toLowerCase();
    if (normalized === "true" || normalized === "1") return true;
    if (normalized === "false" || normalized === "0") return false;
  }
  return undefined;
}

function toNumberArray(value: unknown): number[] | undefined {
  if (!Array.isArray(value)) return undefined;
  const out = value
    .map((entry) => toNumber(entry))
    .filter((entry): entry is number => entry !== undefined);
  return out.length ? out : undefined;
}

function average(values: Array<number | undefined>): number | undefined {
  const valid = values.filter((v): v is number => v !== undefined);
  if (!valid.length) return undefined;
  return valid.reduce((sum, value) => sum + value, 0) / valid.length;
}

function maxDefined(values: Array<number | undefined>): number | undefined {
  const valid = values.filter((v): v is number => v !== undefined);
  if (!valid.length) return undefined;
  return Math.max(...valid);
}

function buildNormalizedSensorRoutes(parsed: ParsedRecord, carType: string): Record<string, unknown> {
  const dynamics = (parsed.dynamics as ParsedRecord | undefined) || {};
  const controls = (parsed.controls as ParsedRecord | undefined) || {};
  const pack = (parsed.pack as ParsedRecord | undefined) || {};
  const diagnostics = (parsed.diagnostics as ParsedRecord | undefined) || {};
  const diagnosticsHigh = (parsed.diagnosticsHigh as ParsedRecord | undefined) || {};
  const diagnosticsLow = (parsed.diagnosticsLow as ParsedRecord | undefined) || {};
  const thermal = (parsed.thermal as ParsedRecord | undefined) || {};

  const hvSoc = toNumber(firstDefined(pack, ["hvSoc"]));
  const hvPackV = toNumber(firstDefined(pack, ["hvPackV"]));
  const hvCurrent = toNumber(firstDefined(pack, ["hvC", "dcBusCurrent"]));
  const lvV = toNumber(firstDefined(pack, ["lvV", "lvBattV"]));
  const timeSinceOnS = toNumber(firstDefined(pack, ["timeSinceOn"]));
  const battery = hvSoc ?? (hvPackV !== undefined ? Math.max(0, Math.min(100, (hvPackV / 600) * 100)) : undefined);
  const odometer = toNumber(firstDefined(diagnostics, ["odometer"]));

  const steerV = toNumber(firstDefined(controls, ["steerV"]));
  const steerColAngle = toNumber(firstDefined(dynamics, ["steerColAngle"]));
  const flwSpeed = toNumber(firstDefined(dynamics, ["flwSpeed"]));
  const frwSpeed = toNumber(firstDefined(dynamics, ["frwSpeed"]));
  const blwSpeed = toNumber(firstDefined(dynamics, ["blwSpeed"]));
  const brwSpeed = toNumber(firstDefined(dynamics, ["brwSpeed"]));
  const wheelSpeedAvg = average([flwSpeed, frwSpeed, blwSpeed, brwSpeed]);
  const speed = toNumber(firstDefined(dynamics, ["dashSpeed", "wheelSpeed"])) ?? wheelSpeedAvg;

  const throttlePct =
    toNumber(firstDefined(dynamics, ["accelPedalTravel"])) ??
    toNumber(firstDefined(controls, ["apps1Travel"])) ??
    (() => {
      const apps1V = toNumber(firstDefined(controls, ["apps1V"]));
      return apps1V !== undefined ? Math.max(0, Math.min(100, (apps1V / 5) * 100)) : undefined;
    })();

  const brakePct =
    toNumber(firstDefined(controls, ["brakePressureF"])) ??
    (() => {
      const bse1V = toNumber(firstDefined(controls, ["bse1V"]));
      return bse1V !== undefined ? Math.max(0, Math.min(100, (bse1V / 5) * 100)) : undefined;
    })();

  const gpsPair = toNumberPair(firstDefined(dynamics, ["gps"]));
  const packetId = toNumber(firstDefined(parsed, ["packetId", "packet_id"]));

  const ambientTempC = toNumber(firstDefined(thermal, ["ambientTemp"]));
  const inverterTempC = toNumber(firstDefined(thermal, ["inverterTemp"]));
  const motorTempC = toNumber(firstDefined(thermal, ["motorTemp"]));
  const coolantTempC = toNumber(
    firstDefined(thermal, ["coolantTemp", "waterMotorTemp", "waterInverterTemp", "waterRadTemp"]),
  );
  const cellsTempArray =
    toNumberArray(firstDefined(thermal, ["cellsTemp"])) ??
    toNumberArray(firstDefined(pack, ["cellsTemps"]));
  const cellsMaxTempC = cellsTempArray?.length ? Math.max(...cellsTempArray) : undefined;
  const battTempC =
    toNumber(firstDefined(thermal, ["battLoopBattTemp", "cellTopTemp", "cellBottomTemp"])) ??
    cellsMaxTempC;
  const hottestTempC = maxDefined([
    ambientTempC,
    inverterTempC,
    motorTempC,
    coolantTempC,
    battTempC,
  ]);
  const thermalHeadroomC =
    hottestTempC !== undefined ? Math.max(-20, 90 - hottestTempC) : undefined;
  const thermalFanSpeed = toNumber(
    firstDefined(thermal, ["radFanRpm", "motorLoopRadFanSpeed", "battLoopRadFanSpeed"]),
  );

  const powerKw =
    hvPackV !== undefined && hvCurrent !== undefined
      ? (hvPackV * hvCurrent) / 1000
      : undefined;

  const shutdownLegs = {
    leg1: toBoolean(firstDefined(diagnosticsLow, ["shutdownLeg1"])),
    leg2: toBoolean(firstDefined(diagnosticsLow, ["shutdownLeg2"])),
    leg3: toBoolean(firstDefined(diagnosticsLow, ["shutdownLeg3"])),
    leg4: toBoolean(firstDefined(diagnosticsLow, ["shutdownLeg4"])),
    leg5: toBoolean(firstDefined(diagnosticsLow, ["shutdownLeg5"])),
    leg6: toBoolean(firstDefined(diagnosticsLow, ["shutdownLeg6"])),
    leg7: toBoolean(firstDefined(diagnosticsLow, ["shutdownLeg7"])),
    leg8: toBoolean(firstDefined(diagnosticsLow, ["shutdownLeg8"])),
    leg9: toBoolean(firstDefined(diagnosticsLow, ["shutdownLeg9"])),
    leg10: toBoolean(firstDefined(diagnosticsLow, ["shutdownLeg10"])),
    leg11: toBoolean(firstDefined(diagnosticsLow, ["shutdownLeg11"])),
    leg12: toBoolean(firstDefined(diagnosticsLow, ["shutdownLeg12"])),
  };
  const shutdownLegValues = Object.values(shutdownLegs).filter(
    (value): value is boolean => value !== undefined,
  );
  const shutdownHealthy = shutdownLegValues.length
    ? shutdownLegValues.every(Boolean)
    : undefined;

  return {
    car_visualization: {
      car_type: carType,
      dynamics: {
        flwSpeed,
        frwSpeed,
        blwSpeed,
        brwSpeed,
      },
    },
    driver_input_visualizer: {
      car_type: carType,
      controls: {
        steerV,
        steerColAngle,
        throttlePct,
        brakePct,
      },
    },
    map: {
      car_type: carType,
      dynamics: {
        gps: gpsPair,
      },
    },
    live_banner: {
      car_type: carType,
      battery,
      odometer,
    },
    dashboard_screen: {
      car_type: carType,
      packetId,
      speed,
      wheelSpeedAvg,
      steerColAngle,
      throttlePct,
      brakePct,
      batteryPct: battery,
      hvPackV,
      hvCurrent,
      lvV,
      inverterTempC,
      motorTempC,
      ambientTempC,
    },
    timing_deltas: {
      car_type: carType,
      packetId,
      speed,
      throttlePct,
      brakePct,
    },
    shutdown_screen: {
      car_type: carType,
      contactorState: toNumber(firstDefined(pack, ["contactorState"])),
      hvcStateMachine: toNumber(firstDefined(diagnosticsHigh, ["hvcStateMachine"])),
      shutdownCurrent: toNumber(firstDefined(diagnosticsHigh, ["shutdownCurrent"])),
      r2dAuthorized: toBoolean(firstDefined(diagnosticsLow, ["r2dAuthorized"])),
      r2dStatus: toBoolean(firstDefined(diagnosticsLow, ["r2dStatus"])),
      negHvContactor: toBoolean(firstDefined(diagnosticsHigh, ["negHvContactor"])),
      posHvContactor: toBoolean(firstDefined(diagnosticsHigh, ["posHvContactor"])),
      prechargeContactor: toBoolean(firstDefined(diagnosticsHigh, ["prechargeContactor"])),
      shutdownHealthy,
      legs: shutdownLegs,
    },
    thermal_headroom: {
      car_type: carType,
      ambientTempC,
      coolantTempC,
      inverterTempC,
      motorTempC,
      battTempC,
      hottestTempC,
      thermalHeadroomC,
      fanSpeed: thermalFanSpeed,
    },
    energy_budget: {
      car_type: carType,
      batteryPct: battery,
      hvPackV,
      hvCurrent,
      powerKw,
      lvV,
      timeSinceOnS,
    },
  };
}

function emitRoutedEvent(
  topic: string,
  partition: number,
  offset: string,
  timestamp: string,
  headers: Record<string, string | undefined>,
  data: unknown,
) {
  const routed: KafkaEvent = {
    topic,
    partition,
    payload: typeof data === "string" ? data : JSON.stringify(data),
    headers,
    offset,
    timestamp,
  };
  bus.emit(`kafka:${topic}` as const, routed);
  bufferMessage(topic, routed);
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
  if (!topic || /[.*+?^${}()|[\]\\]/.test(topic)) return;
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

export function resolveRawTopicsForRequested(requestedTopics: string[]): string[] {
  const cfg = loadRouteConfig();
  const rawTopics = new Set<string>();

  for (const requested of requestedTopics) {
    const topic = requested.trim();
    if (!topic) continue;
    if (NORMALIZED_SENSOR_TOPICS.has(topic)) {
      rawTopics.add(SENSOR_DATA_TOPIC);
      continue;
    }
    if (cfg[topic]?.length) {
      rawTopics.add(topic);
      continue;
    }

    const sourceTopics = Object.entries(cfg)
      .filter(([, rules]) => rules.some((rule) => rule.to === topic))
      .map(([sourceTopic]) => sourceTopic);

    if (sourceTopics.length) {
      sourceTopics.forEach((t) => rawTopics.add(t));
    } else {
      rawTopics.add(topic);
    }
  }

  return Array.from(rawTopics);
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

    const defaultTopics = (process.env.KAFKA_TOPICS || process.env.KAFKA_TOPIC || "")
      .split(",")
      .map((t) => t.trim())
      .filter(Boolean);
    console.log("Default topics to subscribe:", defaultTopics);
    for (const t of defaultTopics) {
      await ensureSubscribe(t);
    }

    // KafkaJS can only subscribe before consumer.run(), so the trackside-live
    // feed's enriched topics (grafana_data_<car>_derived) must be subscribed here
    // at startup rather than on-demand from the SSE route. Same goes for car_status
    // (the classifier's output, read by the car-status SSE route) — an on-demand
    // ensureSubscribe after run() is a silent no-op, so it must be in this set too.
    // Best-effort: a missing or uncreatable topic (e.g. a car with no enricher) must
    // never break the core feed. Override the set via KAFKA_LIVE_TOPICS.
    const liveTopics = (process.env.KAFKA_LIVE_TOPICS || "grafana_data_orion_derived,grafana_data_angelique_derived,car_status")
      .split(",")
      .map((t) => t.trim())
      .filter((t) => t && !defaultTopics.includes(t));
    for (const t of liveTopics) {
      try {
        await ensureSubscribe(t);
      } catch (e) {
        console.warn("Skipped live topic subscription (non-fatal):", t, e);
      }
    }

    await consumer.run({
      eachMessage: async ({ topic, partition, message }) => {
        if (MAX_MESSAGE_AGE_MS > 0) {
          const timestampMs = Number(message.timestamp);
          if (Number.isFinite(timestampMs) && Date.now() - timestampMs > MAX_MESSAGE_AGE_MS) {
            return;
          }
        }

        let payload = message.value ? message.value.toString() : "";
        let parsed: unknown = parsePossiblyJson(payload);

        const headers: Record<string, string | undefined> = Object.fromEntries(
          Object.entries(message.headers || {}).map(([k, v]) => [k, v?.toString()]),
        );
        const headerCarType = normalizeCarType(headers.car_type);
        if (headerCarType) {
          headers.car_type = headerCarType;
        }

        if (topic === SENSOR_DATA_TOPIC && message.value) {
          const decoded = await decodeSensorMessageByCar(
            Buffer.from(message.value),
            headerCarType,
          );
          if (decoded) {
            parsed = decoded;
            payload = JSON.stringify(decoded);
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

        bus.emit(`kafka:${topic}` as const, evt);
        bufferMessage(topic, evt);
        bus.emit("kafka:*", evt);

        if (topic === SENSOR_DATA_TOPIC && parsed && typeof parsed === "object" && !Array.isArray(parsed)) {
          const parsedObj = parsed as ParsedRecord;
          const payloadCarType =
            normalizeCarType((parsedObj.carType as string | undefined) ?? (parsedObj.car_type as string | undefined));
          const normalizedCar = headerCarType ?? payloadCarType ?? "unknown";
          if (normalizedCar !== "unknown") {
            headers.car_type = normalizedCar;
          }

          const normalizedRoutes = buildNormalizedSensorRoutes(parsedObj, normalizedCar);
          for (const [logicalTopic, data] of Object.entries(normalizedRoutes)) {
            emitRoutedEvent(
              logicalTopic,
              partition,
              message.offset,
              message.timestamp,
              headers,
              data,
            );
          }
        }

        const routes = loadRouteConfig()[topic];
        if (routes && routes.length) {
          for (const rule of routes) {
            let outData: unknown = parsed;

            if (parsed && typeof parsed === "object" && !Array.isArray(parsed)) {
              if (rule.pick && rule.pick.length) {
                const subset: Record<string, unknown> = {};
                for (const key of rule.pick) {
                  const val = getByPath(parsed, key);
                  if (val === undefined) continue;

                  const parts = key.split(".").filter(Boolean);
                  if (rule.rename && parts.length) {
                    const leaf = parts[parts.length - 1];
                    const renamedLeaf = rule.rename[leaf];
                    if (renamedLeaf) {
                      parts[parts.length - 1] = renamedLeaf;
                    }
                  }
                  setByPath(subset, parts, val);
                }
                outData = subset;
              } else if (rule.rename && Object.keys(rule.rename).length) {
                const renamed: Record<string, unknown> = {};
                for (const [k, v] of Object.entries(parsed as Record<string, unknown>)) {
                  const newKey = rule.rename[k] || k;
                  renamed[newKey] = v;
                }
                outData = renamed;
              }
            }

            emitRoutedEvent(
              rule.to,
              partition,
              message.offset,
              message.timestamp,
              headers,
              outData,
            );
          }
        }
      },
    });

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
    await ensureTopicExists(topic);
    try {
      await consumer.subscribe({ topic, fromBeginning: false });
      subscribedTopics.add(topic);
      console.log("Subscribed to topic:", topic);
    } catch (e: any) {
      if (e?.type === "UNKNOWN_TOPIC_OR_PARTITION") {
        console.warn("Topic unknown, retrying after create:", topic);
        await ensureTopicExists(topic);
        await consumer.subscribe({ topic, fromBeginning: false });
        subscribedTopics.add(topic);
        console.log("Subscribed on retry:", topic);
      } else {
        throw e;
      }
    }
    return;
  }

  const key = topic.toString();
  if (subscribedRegex.has(key)) return;
  await consumer.subscribe({ topic, fromBeginning: false });
  subscribedRegex.add(key);
  console.log("Subscribed (regex):", key);
}

export function ensureSubscribePrefix(base: string): Promise<void> {
  const regex = new RegExp(`^${escapeRegex(base)}(?:\/.*)?$`);
  return ensureSubscribe(regex);
}

function escapeRegex(s: string): string {
  return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}
