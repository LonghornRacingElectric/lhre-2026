import { NextRequest, NextResponse } from "next/server";
import { services } from "@/lib/motec/service";
import type { ExportRequest } from "@/lib/motec/types";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

export async function POST(req: NextRequest) {
  try {
    const body = (await req.json()) as ExportRequest;
    const { exporter } = services(body.car);
    const result = await exporter.export(body);
    return NextResponse.json(result);
  } catch (err) {
    return NextResponse.json({ error: String(err instanceof Error ? err.message : err) }, { status: 400 });
  }
}
