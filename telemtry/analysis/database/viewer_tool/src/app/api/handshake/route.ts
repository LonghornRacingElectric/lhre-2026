import { NextRequest, NextResponse } from "next/server";
import prismaTelemtry from "@/lib/prisma/telemtry";
import {
  findLatestPacketId,
  normalizeCar,
  resolveCarFromCarId,
} from "@/lib/prisma/carPrisma";

export const dynamic = "force-dynamic";

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

  try {
    let car = normalizeCar(searchParams.get("car"));

    if (!car) {
      const latestDay = await prismaTelemtry.drive_day.findFirst({
        orderBy: { day_id: "desc" },
        select: {
          car_id: true,
          car: { select: { car_name: true } },
        },
      });

      car =
        normalizeCar(latestDay?.car?.car_name) ??
        (await resolveCarFromCarId(latestDay?.car_id)) ??
        "angelique";
    }

    const latestPacketId = await findLatestPacketId(car);
    const last_packet =
      latestPacketId != null ? latestPacketId.toString() : 0;
    return NextResponse.json({ time, last_packet, car });
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
