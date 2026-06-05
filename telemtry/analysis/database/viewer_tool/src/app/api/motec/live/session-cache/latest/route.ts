export const runtime = "nodejs";
export const dynamic = "force-dynamic";

import { NextResponse } from "next/server";
import { promises as fs } from "fs";
import path from "path";

// GET the most recently cached live session (reference: /api/live/session-cache/latest).
function cachePath(): string {
  const dir = process.env.CACHE_DIR
    ? path.join(process.env.CACHE_DIR, "live_sessions")
    : path.join(process.cwd(), ".cache", "live_sessions");
  return path.join(dir, "latest.json");
}

export async function GET() {
  try {
    const raw = await fs.readFile(cachePath(), "utf-8");
    return NextResponse.json(JSON.parse(raw));
  } catch {
    return NextResponse.json({ detail: "No saved live session." }, { status: 404 });
  }
}
