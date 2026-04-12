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
      take: 500,
    });

    // Batch-fetch lookup tables for the IDs present in this result set
    const carIds = [...new Set(rows.map((e) => e.car_id))];
    const driverIds = [...new Set(rows.map((e) => e.driver_id))];
    const locationIds = [...new Set(rows.map((e) => e.location_id))];
    const eventTypeIds = [...new Set(rows.map((e) => e.event_type))];

    const [cars, drivers, locations, eventTypes] = await Promise.all([
      prisma.lut_car.findMany({ where: { car_id: { in: carIds } }, select: { car_id: true, car_name: true } }),
      prisma.lut_driver.findMany({ where: { driver_id: { in: driverIds } }, select: { driver_id: true, driver_name: true } }),
      prisma.lut_location.findMany({ where: { location_id: { in: locationIds } }, select: { location_id: true, area: true, track: true } }),
      prisma.lut_event_type.findMany({ where: { type_id: { in: eventTypeIds } }, select: { type_id: true, event_type: true } }),
    ]);

    const carMap = Object.fromEntries(cars.map((c) => [c.car_id, c.car_name]));
    const driverMap = Object.fromEntries(drivers.map((d) => [d.driver_id, d.driver_name]));
    const locationMap = Object.fromEntries(locations.map((l) => [l.location_id, { area: l.area, track: l.track }]));
    const eventTypeMap = Object.fromEntries(eventTypes.map((et) => [et.type_id, et.event_type]));

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
      driver_name: driverMap[e.driver_id] ?? null,
      area: locationMap[e.location_id]?.area ?? null,
      track: locationMap[e.location_id]?.track ?? null,
      event_type: eventTypeMap[e.event_type] ?? null,
      car_name: carMap[e.car_id] ?? null,
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
