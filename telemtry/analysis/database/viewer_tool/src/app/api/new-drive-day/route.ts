import { NextRequest, NextResponse } from 'next/server';
import prisma from '@/lib/prisma/telemtry';

function toInt(v: unknown): number | undefined {
  if (v === undefined || v === null || v === '') return undefined;
  const n = parseInt(String(v), 10);
  return Number.isFinite(n) ? n : undefined;
}

function toFloat(v: unknown): number | undefined {
  if (v === undefined || v === null || v === '') return undefined;
  const n = parseFloat(String(v));
  return Number.isFinite(n) ? n : undefined;
}

export async function POST(req: NextRequest) {
  try {
    const body = await req.json();
    const {
      // Drive day info
      date,
      track_name,
      weather,
      wind_speed,
      power_limit,
      air_temperature,
      relative_humidity,
      track_temperature,
      // Driver, car, location, event type
      car_id,
      driver_id,
      location_id,
      event_type,
      // Car setup
      car_weight,
      tow_angle,
      camber_front, camber_rear,
      toe_front, toe_rear,
      ride_height_front, ride_height_rear,
      torque_limit,
      // Shock damping per corner
      fr_lsc, fr_lsr, fr_hsc, fr_hsr,
      fl_lsc, fl_lsr, fl_hsc, fl_hsr,
      rr_lsc, rr_lsr, rr_hsc, rr_hsr,
      rl_lsc, rl_lsr, rl_hsc, rl_hsr,
      // Tire cold pressures
      frw_pressure, flw_pressure, brw_pressure, blw_pressure,
      // Tire hot pressures
      frw_hot_pressure, flw_hot_pressure, brw_hot_pressure, blw_hot_pressure,
      // Tire wear depth and durometer
      fr_wear_depth, fl_wear_depth, rr_wear_depth, rl_wear_depth,
      fr_durometer, fl_durometer, rr_durometer, rl_durometer,
      // Aero
      front_wing_on, rear_wing_on,
      front_wing_pitch, rear_wing_pitch,
      regen_on, undertray_on,
      // Springs
      front_roll_spring_rate, front_heave_spring_rate,
      rear_roll_spring_rate, rear_heave_spring_rate,
      front_corner_spring_rate, rear_corner_spring_rate,
      // ARB
      front_arb_setting, rear_arb_setting,
    } = body;

    const dayDate = date ? new Date(date) : new Date();
    const now = BigInt(Date.now());

    // Determine packet_start from the last drive day's packet_end
    const lastDay = await prisma.drive_day.findFirst({
      orderBy: { day_id: 'desc' },
      select: { packet_end: true },
    });
    const packet_start = lastDay?.packet_end ?? BigInt(1);

    const newDay = await prisma.drive_day.create({
      data: {
        date: dayDate,
        track_name: track_name ?? null,
        weather: weather ?? null,
        wind_speed: toFloat(wind_speed) ?? null,
        power_limit: toInt(power_limit) ?? null,
        air_temperature: toFloat(air_temperature) ?? null,
        relative_humidity: toFloat(relative_humidity) ?? null,
        track_temperature: toFloat(track_temperature) ?? null,
        status: 2,
        creation_time: now,
        start_time: now,
        packet_start,
        car_id: toInt(car_id) ?? null,
        driver_id: toInt(driver_id) ?? null,
        location_id: toInt(location_id) ?? 0,
        event_type: toInt(event_type) ?? 0,
        car_weight: toInt(car_weight) ?? null,
        tow_angle: toFloat(tow_angle) ?? null,
        camber_front: toFloat(camber_front) ?? null,
        camber_rear: toFloat(camber_rear) ?? null,
        toe_front: toFloat(toe_front) ?? null,
        toe_rear: toFloat(toe_rear) ?? null,
        ride_height_front: toFloat(ride_height_front) ?? null,
        ride_height_rear: toFloat(ride_height_rear) ?? null,
        torque_limit: toInt(torque_limit) ?? null,
        fr_lsc: toInt(fr_lsc) ?? null, fr_lsr: toInt(fr_lsr) ?? null,
        fr_hsc: toInt(fr_hsc) ?? null, fr_hsr: toInt(fr_hsr) ?? null,
        fl_lsc: toInt(fl_lsc) ?? null, fl_lsr: toInt(fl_lsr) ?? null,
        fl_hsc: toInt(fl_hsc) ?? null, fl_hsr: toInt(fl_hsr) ?? null,
        rr_lsc: toInt(rr_lsc) ?? null, rr_lsr: toInt(rr_lsr) ?? null,
        rr_hsc: toInt(rr_hsc) ?? null, rr_hsr: toInt(rr_hsr) ?? null,
        rl_lsc: toInt(rl_lsc) ?? null, rl_lsr: toInt(rl_lsr) ?? null,
        rl_hsc: toInt(rl_hsc) ?? null, rl_hsr: toInt(rl_hsr) ?? null,
        frw_pressure: toFloat(frw_pressure) ?? null,
        flw_pressure: toFloat(flw_pressure) ?? null,
        brw_pressure: toFloat(brw_pressure) ?? null,
        blw_pressure: toFloat(blw_pressure) ?? null,
        frw_hot_pressure: toFloat(frw_hot_pressure) ?? null,
        flw_hot_pressure: toFloat(flw_hot_pressure) ?? null,
        brw_hot_pressure: toFloat(brw_hot_pressure) ?? null,
        blw_hot_pressure: toFloat(blw_hot_pressure) ?? null,
        fr_wear_depth: toFloat(fr_wear_depth) ?? null,
        fl_wear_depth: toFloat(fl_wear_depth) ?? null,
        rr_wear_depth: toFloat(rr_wear_depth) ?? null,
        rl_wear_depth: toFloat(rl_wear_depth) ?? null,
        fr_durometer: toFloat(fr_durometer) ?? null,
        fl_durometer: toFloat(fl_durometer) ?? null,
        rr_durometer: toFloat(rr_durometer) ?? null,
        rl_durometer: toFloat(rl_durometer) ?? null,
        front_wing_on: front_wing_on ?? null,
        rear_wing_on: rear_wing_on ?? null,
        front_wing_pitch: toFloat(front_wing_pitch) ?? null,
        rear_wing_pitch: toFloat(rear_wing_pitch) ?? null,
        regen_on: regen_on ?? null,
        undertray_on: undertray_on ?? null,
        front_roll_spring_rate: toFloat(front_roll_spring_rate) ?? null,
        front_heave_spring_rate: toFloat(front_heave_spring_rate) ?? null,
        rear_roll_spring_rate: toFloat(rear_roll_spring_rate) ?? null,
        rear_heave_spring_rate: toFloat(rear_heave_spring_rate) ?? null,
        front_corner_spring_rate: toFloat(front_corner_spring_rate) ?? null,
        rear_corner_spring_rate: toFloat(rear_corner_spring_rate) ?? null,
        front_arb_setting: front_arb_setting ?? null,
        rear_arb_setting: rear_arb_setting ?? null,
      },
      select: { day_id: true },
    });

    return NextResponse.json({ day_id: newDay.day_id }, { status: 201 });
  } catch (error) {
    console.error('Error creating new drive day:', error);
    return NextResponse.json({ error: 'Failed to create new drive day' }, { status: 500 });
  } finally {
    await prisma.$disconnect();
  }
}
