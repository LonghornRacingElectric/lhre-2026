import { NextRequest, NextResponse } from "next/server";
import { services } from "@/lib/motec/service";
import type { ChannelChartDefinition } from "@/lib/motec/types";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

export async function GET() {
  try {
    const { channelCharts } = services("orion");
    return NextResponse.json({ charts: await channelCharts.list() });
  } catch (err) {
    return NextResponse.json({ error: String(err instanceof Error ? err.message : err) }, { status: 500 });
  }
}

export async function POST(req: NextRequest) {
  try {
    const { channelCharts } = services("orion");
    const body = (await req.json()) as ChannelChartDefinition;
    return NextResponse.json(await channelCharts.save(body));
  } catch (err) {
    return NextResponse.json({ error: String(err instanceof Error ? err.message : err) }, { status: 400 });
  }
}
