import { NextRequest, NextResponse } from "next/server";
import prismaAngelique from "@/lib/prisma/angelique";

export const dynamic = "force-dynamic";

function normalizeCarName(value: string | null): "angelique" | "orion" | null {
  if (!value) return null;
  const v = value.trim().toLowerCase();
  if (v === "angelique") return "angelique";
  if (v === "orion") return "orion";
  return null;
}

function isMissingPacketTableError(err: unknown): boolean {
  const message = err instanceof Error ? err.message : String(err);
  // Prisma can wrap Postgres errors; this keeps behavior aligned with old code.
  return (
    message.includes("packet") &&
    (message.includes("does not exist") ||
      message.includes("undefined_table") ||
      message.includes("42P01") ||
      message.includes("P2021"))
  );
}

export async function GET(req: NextRequest) {
  const time = Date.now();

  const { searchParams } = new URL(req.url);
  // Backward compatible default: if omitted, use Angelique.
  const car = normalizeCarName(searchParams.get("car")) ?? "angelique";

  try {
    let latest;
    switch(car) {
      case "angelique":
        latest = await prismaAngelique.packet.findFirst({
          orderBy: { packet_id: "desc" },
          select: { packet_id: true },
        });
        break;
    }

    const last_packet =
      latest?.packet_id != null ? latest.packet_id.toString() : 0;
    return NextResponse.json({ time, last_packet });
  } catch (e) {
    console.error("Error in handshake:", e);
    if (isMissingPacketTableError(e)) {
      return NextResponse.json({ time: Date.now(), last_packet: 0 });
    }
    return NextResponse.json(
      { error: "Failed to perform handshake" },
      { status: 500 },
    );
  }
}
