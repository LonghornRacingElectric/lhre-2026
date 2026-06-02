import { NextResponse } from "next/server";
import { getSettings } from "@/lib/motec/config";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

export async function GET() {
  const settings = getSettings("orion");
  return NextResponse.json({
    ok: true,
    source: settings.telemetrySource,
    postgres_enabled: settings.usePostgres,
  });
}
