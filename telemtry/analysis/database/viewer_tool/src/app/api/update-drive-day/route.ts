// Update an existing drive_day's SETUP fields (conditions / car / shock / tires
// / aero) after creation — so editing the Drive Day Setup in trackside-live
// during a session persists. Targets the given day_id, else the active (status=2)
// day. Mirrors /api/new-drive-day's field conversions. Same Prisma client.
export const runtime = "nodejs";
export const dynamic = "force-dynamic";

import { NextRequest, NextResponse } from "next/server";
import prisma from "@/lib/prisma/telemtry";

function toInt(v: unknown): number | undefined {
  if (v === undefined || v === null || v === "") return undefined;
  const n = parseInt(String(v), 10);
  return Number.isFinite(n) ? n : undefined;
}
function toFloat(v: unknown): number | undefined {
  if (v === undefined || v === null || v === "") return undefined;
  const n = parseFloat(String(v));
  return Number.isFinite(n) ? n : undefined;
}

export async function POST(req: NextRequest) {
  try {
    const b = (await req.json().catch(() => ({}))) as Record<string, unknown>;
    let dayId = toInt(b.day_id);
    if (dayId == null) {
      const active = await prisma.drive_day.findFirst({ where: { status: 2 }, orderBy: { day_id: "desc" }, select: { day_id: true } });
      if (!active) return NextResponse.json({ message: "no active drive day" }, { status: 204 });
      dayId = active.day_id;
    }

    const data = {
      track_name: (b.track_name as string) ?? undefined,
      weather: (b.weather as string) ?? undefined,
      wind_speed: toFloat(b.wind_speed), air_temperature: toFloat(b.air_temperature),
      relative_humidity: toFloat(b.relative_humidity), track_temperature: toFloat(b.track_temperature),
      car_weight: toInt(b.car_weight), tow_angle: toFloat(b.tow_angle), torque_limit: toInt(b.torque_limit),
      camber_front: toFloat(b.camber_front), camber_rear: toFloat(b.camber_rear),
      toe_front: toFloat(b.toe_front), toe_rear: toFloat(b.toe_rear),
      ride_height_front: toFloat(b.ride_height_front), ride_height_rear: toFloat(b.ride_height_rear),
      fr_lsc: toInt(b.fr_lsc), fr_lsr: toInt(b.fr_lsr), fr_hsc: toInt(b.fr_hsc), fr_hsr: toInt(b.fr_hsr),
      fl_lsc: toInt(b.fl_lsc), fl_lsr: toInt(b.fl_lsr), fl_hsc: toInt(b.fl_hsc), fl_hsr: toInt(b.fl_hsr),
      rr_lsc: toInt(b.rr_lsc), rr_lsr: toInt(b.rr_lsr), rr_hsc: toInt(b.rr_hsc), rr_hsr: toInt(b.rr_hsr),
      rl_lsc: toInt(b.rl_lsc), rl_lsr: toInt(b.rl_lsr), rl_hsc: toInt(b.rl_hsc), rl_hsr: toInt(b.rl_hsr),
      frw_pressure: toFloat(b.frw_pressure), flw_pressure: toFloat(b.flw_pressure),
      brw_pressure: toFloat(b.brw_pressure), blw_pressure: toFloat(b.blw_pressure),
      frw_hot_pressure: toFloat(b.frw_hot_pressure), flw_hot_pressure: toFloat(b.flw_hot_pressure),
      brw_hot_pressure: toFloat(b.brw_hot_pressure), blw_hot_pressure: toFloat(b.blw_hot_pressure),
      fr_wear_depth: toFloat(b.fr_wear_depth), fl_wear_depth: toFloat(b.fl_wear_depth),
      rr_wear_depth: toFloat(b.rr_wear_depth), rl_wear_depth: toFloat(b.rl_wear_depth),
      fr_durometer: toFloat(b.fr_durometer), fl_durometer: toFloat(b.fl_durometer),
      rr_durometer: toFloat(b.rr_durometer), rl_durometer: toFloat(b.rl_durometer),
      front_wing_on: typeof b.front_wing_on === "boolean" ? b.front_wing_on : undefined,
      rear_wing_on: typeof b.rear_wing_on === "boolean" ? b.rear_wing_on : undefined,
      front_wing_pitch: toFloat(b.front_wing_pitch), rear_wing_pitch: toFloat(b.rear_wing_pitch),
      regen_on: typeof b.regen_on === "boolean" ? b.regen_on : undefined,
      undertray_on: typeof b.undertray_on === "boolean" ? b.undertray_on : undefined,
      front_corner_spring_rate: toFloat(b.front_corner_spring_rate), rear_corner_spring_rate: toFloat(b.rear_corner_spring_rate),
      front_arb_setting: (b.front_arb_setting as string) ?? undefined,
      rear_arb_setting: (b.rear_arb_setting as string) ?? undefined,
    };
    // Drop undefined so we only write provided fields.
    const update = Object.fromEntries(Object.entries(data).filter(([, v]) => v !== undefined));

    await prisma.drive_day.update({ where: { day_id: dayId }, data: update });
    return NextResponse.json({ ok: true, day_id: dayId });
  } catch (error) {
    console.error("update-drive-day error:", error);
    return NextResponse.json({ error: "failed to update drive day" }, { status: 500 });
  }
}
