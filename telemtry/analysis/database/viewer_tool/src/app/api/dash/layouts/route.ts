// Server-side shared library of dash lap-card layouts, so every client sees the
// same saved layouts (not just browser-local). File-backed, same pattern as the
// live session-cache. Last-writer-wins on the whole library (small team, rare
// concurrent edits); the editor pulls on open + on manual refresh.
export const runtime = "nodejs";
export const dynamic = "force-dynamic";

import { NextRequest, NextResponse } from "next/server";
import { promises as fs } from "fs";
import path from "path";

// Per-screen library file. `lapCard` keeps the original library.json (so saved
// lap cards survive this change); any other screen gets library.<screen>.json.
// `screen` is sanitized to a safe slug so the query param can't escape the dir.
function libPath(screen: string): string {
  const dir = process.env.CACHE_DIR
    ? path.join(process.env.CACHE_DIR, "dash_layouts")
    : path.join(process.cwd(), ".cache", "dash_layouts");
  const slug = (screen || "lapCard").replace(/[^a-zA-Z0-9_-]/g, "").slice(0, 40) || "lapCard";
  return path.join(dir, slug === "lapCard" ? "library.json" : `library.${slug}.json`);
}

export async function GET(req: NextRequest) {
  const screen = req.nextUrl.searchParams.get("screen") ?? "lapCard";
  try {
    const raw = await fs.readFile(libPath(screen), "utf-8");
    return NextResponse.json(JSON.parse(raw));
  } catch {
    return NextResponse.json({ items: [], savedAt: 0 });
  }
}

export async function PUT(req: NextRequest) {
  const screen = req.nextUrl.searchParams.get("screen") ?? "lapCard";
  const body = (await req.json().catch(() => ({}))) as { items?: unknown; baseSavedAt?: unknown };
  const items = Array.isArray(body.items) ? body.items : [];
  const baseSavedAt = typeof body.baseSavedAt === "number" ? body.baseSavedAt : null;
  const p = libPath(screen);

  // Optimistic concurrency: if the caller passed the savedAt it loaded and the
  // file has since moved on (another editor saved), reject so the client can
  // merge instead of silently clobbering. First write (no file) always allowed;
  // callers that don't pass baseSavedAt keep the old overwrite behavior.
  let current: { items?: unknown; savedAt?: unknown } | null = null;
  try { current = JSON.parse(await fs.readFile(p, "utf-8")); } catch { /* no file yet */ }
  const currentSavedAt = typeof current?.savedAt === "number" ? current.savedAt : null;
  if (baseSavedAt != null && currentSavedAt != null && baseSavedAt !== currentSavedAt) {
    return NextResponse.json(
      { conflict: true, items: Array.isArray(current?.items) ? current!.items : [], savedAt: currentSavedAt },
      { status: 409 },
    );
  }

  const savedAt = Date.now();
  await fs.mkdir(path.dirname(p), { recursive: true });
  const tmp = `${p}.tmp`;
  await fs.writeFile(tmp, JSON.stringify({ items, savedAt }));
  await fs.rename(tmp, p); // atomic
  return NextResponse.json({ ok: true, count: items.length, savedAt });
}
