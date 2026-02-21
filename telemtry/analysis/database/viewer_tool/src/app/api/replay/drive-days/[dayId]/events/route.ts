import { NextRequest, NextResponse } from "next/server";
import prisma from "@/lib/prisma/telemtry";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

export async function GET(
  _req: NextRequest,
  ctx: { params: Promise<{ dayId: string }> },
) {
  try {
    const { dayId: dayIdParam } = await ctx.params;
    const dayId = Number(dayIdParam);
    if (!Number.isFinite(dayId)) {
      return NextResponse.json({ error: "Missing/invalid 'dayId'" }, { status: 400 });
    }

    const day = await prisma.drive_day.findUnique({
      where: { day_id: dayId },
      select: {
        day_id: true,
        date: true,
        power_limit: true,
        air_temperature: true,
        relative_humidity: true,
        track_temperature: true,
      },
    });

    if (!day) {
      return NextResponse.json({ error: "Drive day not found" }, { status: 404 });
    }

    const rows = await prisma.event.findMany({
      where: { day_id: dayId },
      orderBy: { event_id: "desc" },
      include: {
        driver: { select: { driver_name: true } },
        location: { select: { area: true, track: true } },
        eventType: { select: { event_type: true } },
        car: { select: { car_name: true } },
      },
      take: 500,
    });

    const events = rows.map((e) => ({
      event_id: e.event_id,
      day_id: e.day_id,
      status: e.status,
      creation_time: e.creation_time.toString(),
      start_time: e.start_time != null ? e.start_time.toString() : null,
      end_time: e.end_time != null ? e.end_time.toString() : null,
      packet_start: e.packet_start != null ? e.packet_start.toString() : null,
      packet_end: e.packet_end != null ? e.packet_end.toString() : null,
      day_date: day.date.toISOString(),
      driver_name: e.driver?.driver_name ?? null,
      area: e.location?.area ?? null,
      track: e.location?.track ?? null,
      event_type: e.eventType?.event_type ?? null,
      car_name: e.car?.car_name ?? null,
    }));

    return NextResponse.json(
      {
        drive_day: {
          day_id: day.day_id,
          date: day.date.toISOString(),
          power_limit: day.power_limit ?? null,
          air_temperature: day.air_temperature ?? null,
          relative_humidity: day.relative_humidity ?? null,
          track_temperature: day.track_temperature ?? null,
        },
        events,
      },
      { status: 200 },
    );
  } catch (e) {
    console.error("Failed to list replay drive-day events", e);
    return NextResponse.json(
      { error: "Failed to list replay drive-day events" },
      { status: 500 },
    );
  }
}
