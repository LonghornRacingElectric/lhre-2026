export const runtime = "nodejs";
export const dynamic = "force-dynamic";

import { NextRequest, NextResponse } from "next/server";
import { getSettings } from "@/lib/motec/config";
import { kafkaTopicFor, kafkaTransportFor } from "@/lib/motec/live";

// Native replacement for the reference backend's /api/live/config. Reports the
// resolved live topic + transport so the dashboard status line is accurate.
// MQTT topic resolution mirrors the reference (env LIVE_MQTT_TOPIC, else source).
function mqttTopicFor(source: string, requested?: string | null): string {
  const t = (requested || "").trim();
  if (t) return t;
  return (process.env.LIVE_MQTT_TOPIC || "").trim() || source.trim().toLowerCase();
}

export async function GET(req: NextRequest) {
  const url = new URL(req.url);
  const source = (url.searchParams.get("source") || "orion").trim().toLowerCase();
  const requestedTopic = url.searchParams.get("topic");

  let settings;
  try {
    settings = getSettings(source);
  } catch (e) {
    return NextResponse.json({ detail: e instanceof Error ? e.message : String(e) }, { status: 400 });
  }

  const transport = kafkaTransportFor(url.searchParams.get("transport"));
  const topic =
    transport === "mqtt"
      ? mqttTopicFor(source, requestedTopic)
      : kafkaTopicFor(source, settings, requestedTopic);

  return NextResponse.json({
    source,
    topic,
    transport,
    bootstrap_servers: process.env.KAFKA_BOOTSTRAP_SERVERS || "192.168.1.109:29092",
    mqtt_host: process.env.LIVE_MQTT_HOST || "18.191.225.118",
    mqtt_port: Number(process.env.LIVE_MQTT_PORT) || 1883,
  });
}
