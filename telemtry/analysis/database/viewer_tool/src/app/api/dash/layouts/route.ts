// Server-side shared library of dash lap-card layouts, so every client sees the
// same saved layouts (not just browser-local). File-backed, same pattern as the
// live session-cache. Last-writer-wins on the whole library (small team, rare
// concurrent edits); the editor pulls on open + on manual refresh.
export const runtime = "nodejs";
export const dynamic = "force-dynamic";

import { NextRequest, NextResponse } from "next/server";
import { promises as fs } from "fs";
import path from "path";

function libPath(): string {
  const dir = process.env.CACHE_DIR
    ? path.join(process.env.CACHE_DIR, "dash_layouts")
    : path.join(process.cwd(), ".cache", "dash_layouts");
  return path.join(dir, "library.json");
}

export async function GET() {
  try {
    const raw = await fs.readFile(libPath(), "utf-8");
    return NextResponse.json(JSON.parse(raw));
  } catch {
    return NextResponse.json({ items: [], savedAt: 0 });
  }
}

export async function PUT(req: NextRequest) {
  const body = (await req.json().catch(() => ({}))) as { items?: unknown };
  const items = Array.isArray(body.items) ? body.items : [];
  const p = libPath();
  await fs.mkdir(path.dirname(p), { recursive: true });
  const tmp = `${p}.tmp`;
  await fs.writeFile(tmp, JSON.stringify({ items, savedAt: Date.now() }));
  await fs.rename(tmp, p); // atomic
  return NextResponse.json({ ok: true, count: items.length });
}
