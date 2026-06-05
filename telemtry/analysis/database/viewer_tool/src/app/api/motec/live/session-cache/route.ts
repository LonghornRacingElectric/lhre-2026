export const runtime = "nodejs";
export const dynamic = "force-dynamic";

import { NextRequest, NextResponse } from "next/server";
import { promises as fs } from "fs";
import path from "path";

// Native replacement for the reference backend's /api/live/session-cache. Persists
// the most recent live session so a reload can resume in-progress laps. Stored as a
// single JSON file alongside the other repo-local motec data dirs.
function cachePath(): string {
  const dir = process.env.CACHE_DIR
    ? path.join(process.env.CACHE_DIR, "live_sessions")
    : path.join(process.cwd(), ".cache", "live_sessions");
  return path.join(dir, "latest.json");
}

async function ensureDir(filePath: string): Promise<void> {
  await fs.mkdir(path.dirname(filePath), { recursive: true });
}

export async function GET() {
  const file = cachePath();
  try {
    const raw = await fs.readFile(file, "utf-8");
    return NextResponse.json(JSON.parse(raw));
  } catch {
    return NextResponse.json({ detail: "No saved live session." }, { status: 404 });
  }
}

export async function POST(req: NextRequest) {
  let payload: Record<string, unknown>;
  try {
    payload = await req.json();
  } catch {
    return NextResponse.json({ detail: "Invalid JSON body." }, { status: 400 });
  }
  const saved = { ...payload };
  const savedAt = Number(saved.savedAt) || Date.now();
  saved.savedAt = savedAt;

  const file = cachePath();
  await ensureDir(file);
  const tmp = `${file}.tmp`;
  await fs.writeFile(tmp, JSON.stringify(saved), "utf-8");
  await fs.rename(tmp, file);

  return NextResponse.json({ ok: true, savedAt });
}

export async function DELETE() {
  const file = cachePath();
  await fs.rm(file, { force: true });
  await fs.rm(`${file}.tmp`, { force: true });
  return NextResponse.json({ ok: true });
}
