import { NextResponse } from "next/server";
import { SOURCES } from "@/lib/motec/service";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

export async function GET() {
  return NextResponse.json({ sources: SOURCES });
}
