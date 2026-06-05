export const runtime = "nodejs";
export const dynamic = "force-dynamic";

import { NextRequest, NextResponse } from "next/server";
import { publishJson } from "@/lib/kafka/producer";

const CONFIG_TOPIC = process.env.KAFKA_CAR_STATUS_CONFIG_TOPIC || "car_status_config";

// Known tunable threshold keys (mirrors the processor's Thresholds dataclass).
// We whitelist + coerce so a stray field can never reach the classifier.
const KEYS = ["hv_live_v", "move_rpm", "move_wheel", "move_mps", "min_state_ms", "max_gap_ms"] as const;

/**
 * Publish live car-status threshold overrides to the `car_status_config` Kafka
 * topic. The always-on processor consumes this and re-tunes classification for
 * every car immediately. Body: a partial map of threshold keys -> numbers.
 */
export async function POST(req: NextRequest) {
  let body: Record<string, unknown>;
  try {
    body = await req.json();
  } catch {
    return NextResponse.json({ error: "Invalid JSON body." }, { status: 400 });
  }

  if (typeof body !== "object" || body === null) {
    return NextResponse.json({ error: "Invalid JSON body." }, { status: 400 });
  }

  const overrides: Record<string, number> = {};
  for (const key of KEYS) {
    if (body[key] === undefined || body[key] === null) continue;
    const n = Number(body[key]);
    if (Number.isFinite(n)) overrides[key] = n;
  }
  if (Object.keys(overrides).length === 0) {
    return NextResponse.json({ error: "No valid threshold values provided." }, { status: 400 });
  }

  try {
    await publishJson(CONFIG_TOPIC, overrides);
  } catch (e) {
    return NextResponse.json({ error: String(e instanceof Error ? e.message : e) }, { status: 503 });
  }
  return NextResponse.json({ ok: true, applied: overrides });
}
