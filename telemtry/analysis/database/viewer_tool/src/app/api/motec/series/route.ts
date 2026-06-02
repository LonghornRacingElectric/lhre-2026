import { NextRequest, NextResponse } from "next/server";
import { num, services } from "@/lib/motec/service";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

export async function GET(req: NextRequest) {
  try {
    const p = req.nextUrl.searchParams;
    const { telemetry } = services(p.get("source"));
    const res = await telemetry.series(
      p.get("channel") || "",
      num(p.get("startMs"), 0),
      num(p.get("endMs"), 0),
      num(p.get("maxPoints"), 5000),
    );
    return NextResponse.json(res);
  } catch (err) {
    return NextResponse.json({ error: String(err instanceof Error ? err.message : err) }, { status: 500 });
  }
}
