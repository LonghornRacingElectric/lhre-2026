import { NextRequest, NextResponse } from "next/server";
import { getSettings } from "@/lib/motec/config";
import { ChannelChartStore } from "@/lib/motec/stores";
import { exportLiveLap } from "@/lib/motec/live";
import type { LiveLapExportRequest } from "@/lib/motec/types";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

export async function POST(req: NextRequest) {
  try {
    const body = (await req.json()) as LiveLapExportRequest;
    const settings = getSettings(body.car);
    const charts = new ChannelChartStore(settings.channelChartDir);
    const result = await exportLiveLap(body, settings, charts);
    return NextResponse.json(result);
  } catch (err) {
    return NextResponse.json({ error: String(err instanceof Error ? err.message : err) }, { status: 400 });
  }
}
