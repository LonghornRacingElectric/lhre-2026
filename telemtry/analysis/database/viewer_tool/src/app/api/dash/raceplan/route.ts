// Shared race plan (total laps + usable energy budget) so every trackside client
// sees the same numbers and the energy pacing is computed off one source. Same
// file-backed last-writer-wins pattern as the dash layout/message libraries.
export const runtime = "nodejs";
export const dynamic = "force-dynamic";

import { NextRequest, NextResponse } from "next/server";
import { promises as fs } from "fs";
import path from "path";

function planPath(): string {
  const dir = process.env.CACHE_DIR
    ? path.join(process.env.CACHE_DIR, "dash_raceplan")
    : path.join(process.cwd(), ".cache", "dash_raceplan");
  return path.join(dir, "plan.json");
}

export async function GET() {
  try {
    const raw = await fs.readFile(planPath(), "utf-8");
    return NextResponse.json(JSON.parse(raw));
  } catch {
    return NextResponse.json({ totalLaps: 0, budgetKwh: 0, soeCutoffCellV: 0, savedAt: 0 });
  }
}

export async function PUT(req: NextRequest) {
  const body = (await req.json().catch(() => ({}))) as { totalLaps?: unknown; budgetKwh?: unknown; soeCutoffCellV?: unknown };
  const totalLaps = Number.isFinite(Number(body.totalLaps)) ? Math.max(0, Math.floor(Number(body.totalLaps))) : 0;
  const budgetKwh = Number.isFinite(Number(body.budgetKwh)) ? Math.max(0, Number(body.budgetKwh)) : 0;
  // OCV cutoff is optional; 0 means "unset" so a client that omits it doesn't
  // clobber a previously-shared value with a spurious zero.
  const soeCutoffCellV = Number.isFinite(Number(body.soeCutoffCellV)) ? Math.max(0, Number(body.soeCutoffCellV)) : 0;
  const p = planPath();
  await fs.mkdir(path.dirname(p), { recursive: true });
  const tmp = `${p}.tmp`;
  await fs.writeFile(tmp, JSON.stringify({ totalLaps, budgetKwh, soeCutoffCellV, savedAt: Date.now() }));
  await fs.rename(tmp, p); // atomic
  return NextResponse.json({ ok: true });
}
