import { NextRequest, NextResponse } from "next/server";
import { num, services } from "@/lib/motec/service";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

export async function GET(req: NextRequest) {
  try {
    const p = req.nextUrl.searchParams;
    const { telemetry } = services(p.get("source"));
    const segments = await telemetry.thresholdSegments(
      num(p.get("startMs"), 0),
      num(p.get("endMs"), 0),
      p.get("channel") || undefined,
      num(p.get("threshold"), 0),
      20000,
      num(p.get("minDurationS"), 0),
    );
    return NextResponse.json({ segments });
  } catch (err) {
    return NextResponse.json({ error: String(err instanceof Error ? err.message : err) }, { status: 500 });
  }
}
