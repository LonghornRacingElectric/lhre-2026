// Server-side shared library of driver messages, so every client sees the same
// saved messages + active set (not just browser-local). File-backed, same
// pattern as the lap-card layouts route. Last-writer-wins on the whole library.
export const runtime = "nodejs";
export const dynamic = "force-dynamic";

import { NextRequest, NextResponse } from "next/server";
import { promises as fs } from "fs";
import path from "path";

function libPath(): string {
  const dir = process.env.CACHE_DIR
    ? path.join(process.env.CACHE_DIR, "dash_messages")
    : path.join(process.cwd(), ".cache", "dash_messages");
  return path.join(dir, "library.json");
}

export async function GET() {
  try {
    const raw = await fs.readFile(libPath(), "utf-8");
    return NextResponse.json(JSON.parse(raw));
  } catch {
    return NextResponse.json({ items: [], activeIds: [], savedAt: 0 });
  }
}

export async function PUT(req: NextRequest) {
  const body = (await req.json().catch(() => ({}))) as { items?: unknown; activeIds?: unknown };
  const items = Array.isArray(body.items) ? body.items : [];
  const activeIds = Array.isArray(body.activeIds) ? body.activeIds : [];
  const p = libPath();
  await fs.mkdir(path.dirname(p), { recursive: true });
  const tmp = `${p}.tmp`;
  await fs.writeFile(tmp, JSON.stringify({ items, activeIds, savedAt: Date.now() }));
  await fs.rename(tmp, p); // atomic
  return NextResponse.json({ ok: true, count: items.length });
}
