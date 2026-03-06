import { NextResponse } from "next/server";
import prisma from "@/lib/prisma/telemtry";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

export async function GET() {
  try {
    const rows = await prisma.drive_day.findMany({
      orderBy: { day_id: "desc" },
      take: 50,
      select: {
        day_id: true,
        date: true,
        power_limit: true,
        air_temperature: true,
        relative_humidity: true,
        track_temperature: true,
        _count: { select: { events: true } },
      },
    });

    const drive_days = rows.map((d) => ({
      drive_day: {
        day_id: d.day_id,
        date: d.date.toISOString(),
        power_limit: d.power_limit ?? null,
        air_temperature: d.air_temperature ?? null,
        relative_humidity: d.relative_humidity ?? null,
        track_temperature: d.track_temperature ?? null,
      },
      event_count: d._count.events,
    }));

    return NextResponse.json({ drive_days }, { status: 200 });
  } catch (e) {
    console.error("Failed to list replay drive days", e);
    return NextResponse.json(
      { error: "Failed to list replay drive days" },
      { status: 500 },
    );
  }
}
