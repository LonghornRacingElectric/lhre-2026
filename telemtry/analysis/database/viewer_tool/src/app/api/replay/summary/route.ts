import { NextRequest, NextResponse } from "next/server";
import prismaTelemtry from "@/lib/prisma/telemtry";
import {
  findPacketTimeById,
  normalizeCar,
  resolveCarFromCarId,
} from "@/lib/prisma/carPrisma";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

function addIfPresent(
  out: Record<string, string | number | boolean>,
  key: string,
  value: unknown,
) {
  if (value === null || value === undefined) return;
  if (typeof value === "bigint") {
    out[key] = value.toString();
    return;
  }
  if (
    typeof value === "string" ||
    typeof value === "number" ||
    typeof value === "boolean"
  ) {
    out[key] = value;
    return;
  }
  out[key] = String(value);
}

function bigintToSafeNumber(v: bigint | null): number | null {
  if (v == null) return null;
  const n = Number(v);
  if (!Number.isFinite(n)) return null;
  if (Math.abs(n) > Number.MAX_SAFE_INTEGER) return null;
  return n;
}

export async function GET(req: NextRequest) {
  try {
    const url = new URL(req.url);
    const eventIdRaw = url.searchParams.get("eventId");
    const eventId = eventIdRaw ? Number(eventIdRaw) : NaN;
    if (!Number.isFinite(eventId)) {
      return NextResponse.json(
        { error: "Missing/invalid 'eventId'" },
        { status: 400 },
      );
    }

    // eventId maps to day_id since drive_day is now the single session record
    const ev = await prismaTelemtry.drive_day.findUnique({
      where: { day_id: eventId },
      include: {
        car:       { select: { car_name: true } },
        driver:    { select: { driver_name: true } },
        location:  { select: { area: true, track: true } },
        eventType: { select: { event_type: true } },
      },
    });

    if (!ev)
      return NextResponse.json({ error: "Drive day not found" }, { status: 404 });
    if (ev.packet_start == null || ev.packet_end == null) {
      return NextResponse.json(
        { error: "Drive day has no packet range" },
        { status: 409 },
      );
    }

    const packetStart = BigInt(ev.packet_start as any);
    const packetEnd = BigInt(ev.packet_end as any);
    const requestedCar = normalizeCar(url.searchParams.get("car"));
    const car =
      requestedCar ??
      normalizeCar(ev.car?.car_name) ??
      (await resolveCarFromCarId(ev.car_id)) ??
      "orion";
    const [p0, p1] = await Promise.all([
      findPacketTimeById(car, packetStart),
      findPacketTimeById(car, packetEnd),
    ]);

    const startMs = p0 != null ? BigInt(p0 as any) : null;
    const endMs = p1 != null ? BigInt(p1 as any) : null;

    if (startMs == null || endMs == null) {
      return NextResponse.json(
        { error: "Drive day has no time range" },
        { status: 409 },
      );
    }

    const classifiers = await prismaTelemtry.classifier.findMany({
      where: {
        AND: [
          { day_id: BigInt(eventId) },
          { start_time: { gte: startMs } },
          {
            OR: [{ end_time: null }, { end_time: { lte: endMs } }],
          },
        ],
      },
      orderBy: [{ start_time: "asc" }],
      select: { type: true, start_time: true, end_time: true, notes: true },
    });

    const lap_times: Array<{
      start_time: string;
      end_time: string | null;
      notes: string | null;
    }> = [];
    const flagged_events: Array<{
      type: string;
      start_time: string;
      end_time: string | null;
      notes: string | null;
    }> = [];

    for (const c of classifiers as any[]) {
      const cStartMs = c.start_time != null ? BigInt(c.start_time as any) : null;
      const cEndMs = c.end_time != null ? BigInt(c.end_time as any) : null;

      if (c.type === "lap") {
        lap_times.push({
          start_time: String(cStartMs),
          end_time: cEndMs != null ? String(cEndMs) : null,
          notes: c.notes ?? null,
        });
      } else {
        flagged_events.push({
          type: String(c.type),
          start_time: String(cStartMs),
          end_time: cEndMs != null ? String(cEndMs) : null,
          notes: c.notes ?? null,
        });
      }
    }

    const event_details: Record<string, string | number | boolean> = {};
    addIfPresent(event_details, "creation_time", ev.creation_time);
    addIfPresent(event_details, "start_time", ev.start_time);
    addIfPresent(event_details, "end_time", ev.end_time);
    addIfPresent(event_details, "car_weight", ev.car_weight);
    addIfPresent(event_details, "tow_angle", ev.tow_angle);
    addIfPresent(event_details, "camber_front", ev.camber_front);
    addIfPresent(event_details, "camber_rear", ev.camber_rear);
    addIfPresent(event_details, "toe_front", ev.toe_front);
    addIfPresent(event_details, "toe_rear", ev.toe_rear);
    addIfPresent(event_details, "ride_height_front", ev.ride_height_front);
    addIfPresent(event_details, "ride_height_rear", ev.ride_height_rear);
    addIfPresent(event_details, "ride_height", ev.ride_height);
    addIfPresent(event_details, "ackerman_adjustment", ev.ackerman_adjustment);
    addIfPresent(event_details, "shock_dampening", ev.shock_dampening);
    addIfPresent(event_details, "power_limit", ev.power_limit);
    addIfPresent(event_details, "torque_limit", ev.torque_limit);
    addIfPresent(event_details, "frw_pressure", ev.frw_pressure);
    addIfPresent(event_details, "flw_pressure", ev.flw_pressure);
    addIfPresent(event_details, "brw_pressure", ev.brw_pressure);
    addIfPresent(event_details, "blw_pressure", ev.blw_pressure);
    addIfPresent(event_details, "fr_wear_depth", ev.fr_wear_depth);
    addIfPresent(event_details, "fl_wear_depth", ev.fl_wear_depth);
    addIfPresent(event_details, "rr_wear_depth", ev.rr_wear_depth);
    addIfPresent(event_details, "rl_wear_depth", ev.rl_wear_depth);
    addIfPresent(event_details, "fr_durometer", ev.fr_durometer);
    addIfPresent(event_details, "fl_durometer", ev.fl_durometer);
    addIfPresent(event_details, "rr_durometer", ev.rr_durometer);
    addIfPresent(event_details, "rl_durometer", ev.rl_durometer);
    addIfPresent(event_details, "fr_lsc", ev.fr_lsc);
    addIfPresent(event_details, "fr_lsr", ev.fr_lsr);
    addIfPresent(event_details, "fr_hsc", ev.fr_hsc);
    addIfPresent(event_details, "fr_hsr", ev.fr_hsr);
    addIfPresent(event_details, "fl_lsc", ev.fl_lsc);
    addIfPresent(event_details, "fl_lsr", ev.fl_lsr);
    addIfPresent(event_details, "fl_hsc", ev.fl_hsc);
    addIfPresent(event_details, "fl_hsr", ev.fl_hsr);
    addIfPresent(event_details, "rr_lsc", ev.rr_lsc);
    addIfPresent(event_details, "rr_lsr", ev.rr_lsr);
    addIfPresent(event_details, "rr_hsc", ev.rr_hsc);
    addIfPresent(event_details, "rr_hsr", ev.rr_hsr);
    addIfPresent(event_details, "rl_lsc", ev.rl_lsc);
    addIfPresent(event_details, "rl_lsr", ev.rl_lsr);
    addIfPresent(event_details, "rl_hsc", ev.rl_hsc);
    addIfPresent(event_details, "rl_hsr", ev.rl_hsr);
    addIfPresent(event_details, "front_wing_on", ev.front_wing_on);
    addIfPresent(event_details, "rear_wing_on", ev.rear_wing_on);
    addIfPresent(event_details, "front_wing_pitch", ev.front_wing_pitch);
    addIfPresent(event_details, "rear_wing_pitch", ev.rear_wing_pitch);
    addIfPresent(event_details, "regen_on", ev.regen_on);
    addIfPresent(event_details, "undertray_on", ev.undertray_on);
    addIfPresent(event_details, "front_roll_spring_rate", ev.front_roll_spring_rate);
    addIfPresent(event_details, "front_heave_spring_rate", ev.front_heave_spring_rate);
    addIfPresent(event_details, "rear_roll_spring_rate", ev.rear_roll_spring_rate);
    addIfPresent(event_details, "rear_heave_spring_rate", ev.rear_heave_spring_rate);

    return NextResponse.json(
      {
        event_id: ev.day_id,
        packet_start: packetStart.toString(),
        packet_end: packetEnd.toString(),
        day_id: ev.day_id,
        day_date: ev.date.toISOString(),
        driver_name: ev.driver?.driver_name ?? null,
        area: ev.location?.area ?? null,
        track: ev.location?.track ?? null,
        event_type: ev.eventType?.event_type ?? null,
        car_name: ev.car?.car_name ?? null,
        car_type: car,
        time_start: bigintToSafeNumber(startMs),
        time_end: bigintToSafeNumber(endMs),
        lap_times,
        flagged_events,
        event_details,
      },
      { status: 200 },
    );
  } catch (e) {
    console.error("Failed to load replay summary", e);
    return NextResponse.json(
      { error: "Failed to load replay summary" },
      { status: 500 },
    );
  }
}
