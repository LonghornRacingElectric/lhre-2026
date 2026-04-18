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
      include: {
        car:       { select: { car_name: true } },
        driver:    { select: { driver_name: true } },
        location:  { select: { area: true, track: true } },
        eventType: { select: { event_type: true } },
      },
    });

    if (!day) {
      return NextResponse.json({ error: "Drive day not found" }, { status: 404 });
    }

    // Drive day is the single session — return it shaped like an event for replay compatibility
    const events = [
      {
        event_id: day.day_id,
        day_id: day.day_id,
        status: day.status,
        creation_time: day.creation_time?.toString() ?? null,
        start_time: day.start_time != null ? day.start_time.toString() : null,
        end_time: day.end_time != null ? day.end_time.toString() : null,
        packet_start: day.packet_start != null ? day.packet_start.toString() : null,
        packet_end: day.packet_end != null ? day.packet_end.toString() : null,
        day_date: day.date.toISOString(),
        driver_name: day.driver?.driver_name ?? null,
        area: day.location?.area ?? null,
        track: day.location?.track ?? null,
        event_type: day.eventType?.event_type ?? null,
        car_name: day.car?.car_name ?? null,
      },
    ];

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
