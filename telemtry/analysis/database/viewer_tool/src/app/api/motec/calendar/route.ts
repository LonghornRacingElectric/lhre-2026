import { NextRequest, NextResponse } from "next/server";
import { num, services } from "@/lib/motec/service";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

export async function GET(req: NextRequest) {
  try {
    const p = req.nextUrl.searchParams;
    const { telemetry } = services(p.get("source"));
    const days = await telemetry.calendar(
      p.get("channel"),
      num(p.get("threshold"), 0),
      num(p.get("minDurationS"), 0),
      p.get("validOnly") !== "false",
    );
    return NextResponse.json({ days });
  } catch (err) {
    return NextResponse.json({ error: String(err instanceof Error ? err.message : err) }, { status: 500 });
  }
}
