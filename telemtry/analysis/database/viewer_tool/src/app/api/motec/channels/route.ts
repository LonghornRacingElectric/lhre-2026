import { NextRequest, NextResponse } from "next/server";
import { services } from "@/lib/motec/service";
import { DEFAULT_CHANNEL_KEY } from "@/lib/motec/channels";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

export async function GET(req: NextRequest) {
  try {
    const source = req.nextUrl.searchParams.get("source");
    const { telemetry } = services(source);
    const channels = await telemetry.channels();
    const def = channels.find((c) => c.default)?.key ?? DEFAULT_CHANNEL_KEY;
    return NextResponse.json({ channels, default: def });
  } catch (err) {
    return NextResponse.json({ error: String(err instanceof Error ? err.message : err) }, { status: 500 });
  }
}
