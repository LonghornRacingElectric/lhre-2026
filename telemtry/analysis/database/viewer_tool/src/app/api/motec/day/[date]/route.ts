import { NextRequest, NextResponse } from "next/server";
import { services } from "@/lib/motec/service";
import { DEFAULT_CHANNEL_KEY } from "@/lib/motec/channels";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

export async function GET(req: NextRequest, { params }: { params: Promise<{ date: string }> }) {
  try {
    const { date } = await params;
    const p = req.nextUrl.searchParams;
    const { telemetry } = services(p.get("source"));
    const detail = await telemetry.dayDetail(date, p.get("channel") || DEFAULT_CHANNEL_KEY);
    return NextResponse.json(detail);
  } catch (err) {
    return NextResponse.json({ error: String(err instanceof Error ? err.message : err) }, { status: 500 });
  }
}
