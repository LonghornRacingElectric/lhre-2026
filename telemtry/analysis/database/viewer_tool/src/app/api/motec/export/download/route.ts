import { NextRequest, NextResponse } from "next/server";
import { promises as fs } from "fs";
import path from "path";
import { getSettings } from "@/lib/motec/config";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

export async function GET(req: NextRequest) {
  const id = req.nextUrl.searchParams.get("id");
  if (!id || /[\\/]/.test(id) || id.includes("..")) {
    return NextResponse.json({ error: "Invalid export id" }, { status: 400 });
  }
  const settings = getSettings("orion");
  const zipPath = path.join(settings.exportDir, `${id}.zip`);
  try {
    const data = await fs.readFile(zipPath);
    return new NextResponse(new Uint8Array(data), {
      headers: {
        "Content-Type": "application/zip",
        "Content-Disposition": `attachment; filename="${id}.zip"`,
        "Content-Length": String(data.length),
      },
    });
  } catch {
    return NextResponse.json({ error: "Export not found" }, { status: 404 });
  }
}
