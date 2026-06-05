import { NextRequest, NextResponse } from "next/server";
import { services } from "@/lib/motec/service";
import type { TrackDefinition } from "@/lib/motec/types";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

export async function GET() {
  try {
    const { tracks } = services("orion");
    return NextResponse.json({ tracks: await tracks.list() });
  } catch (err) {
    return NextResponse.json({ error: String(err instanceof Error ? err.message : err) }, { status: 500 });
  }
}

export async function POST(req: NextRequest) {
  try {
    const { tracks } = services("orion");
    const body = (await req.json()) as TrackDefinition;
    return NextResponse.json(await tracks.save(body));
  } catch (err) {
    return NextResponse.json({ error: String(err instanceof Error ? err.message : err) }, { status: 400 });
  }
}
