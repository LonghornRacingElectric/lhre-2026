import { NextRequest, NextResponse } from "next/server";
import prisma from "@/lib/prisma/telemtry"; // adjust path to your prisma client

export async function POST(req: NextRequest) {
  try {
    const body = await req.json();

    // Set status to 2 (created and awaiting start)
    const status = 2;

    // Find the last event to determine packet_start
    const lastEvent = await prisma.event.findFirst({
      where: { status: 0 },
      orderBy: { event_id: "desc" },
      select: { packet_end: true },
    });
    const packet_start = Number(lastEvent?.packet_end ?? 0) + 1;

    // Find the latest drive_day
    const currentDriveDay = await prisma.drive_day.findFirst({
      orderBy: { day_id: "desc" },
      where: { date: new Date() },
      select: { day_id: true },
    });

    if (!currentDriveDay?.day_id) {
      return NextResponse.json(
        { error: "No drive day found" },
        { status: 500 }
      );
    }

    const creation_time = Date.now();

    const {
      eventId,
      driverId,
      locationId,
      eventType,
      carId,
      carWeight,
      towAngle,
      // alignment
      camberFront,
      camberRear,
      toeFront,
      toeRear,
      // ride height
      rideHeightFront,
      rideHeightRear,
      powerLimit,
      // damping
      frLSC,
      frLSR,
      frHSC,
      frHSR,
      flLSC,
      flLSR,
      flHSC,
      flHSR,
      rrLSC,
      rrLSR,
      rrHSC,
      rrHSR,
      rlLSC,
      rlLSR,
      rlHSC,
      rlHSR,
      torqueLimit,
      // pressures
      frPressure,
      flPressure,
      rrPressure,
      rlPressure,
      // tires
      frWearDepth,
      flWearDepth,
      rrWearDepth,
      rlWearDepth,
      frDurometer,
      flDurometer,
      rrDurometer,
      rlDurometer,
      // aero
      frontWingOn,
      rearWingOn,
      frontWingPitch,
      rearWingPitch,
      regenOn,
      undertrayOn,
      // optional special
      frontRollSpringRate,
      frontHeaveSpringRate,
      rearRollSpringRate,
      rearHeaveSpringRate,
      frontCornerSpringRate,
      rearCornerSpringRate,
      frontArbSetting,
      rearArbSetting,
    } = body;

    const toInt = (v: any) =>
      v === "" || v == null
        ? null
        : Number.isFinite(Number(v))
        ? parseInt(v)
        : null;
    const toFloat = (v: any) =>
      v === "" || v == null
        ? null
        : Number.isFinite(Number(v))
        ? parseFloat(v)
        : null;

    const DEFAULT = 0;

    const baseData: any = {
        day_id: currentDriveDay.day_id,
        status,
        creation_time,
        start_time: creation_time,
        packet_start,
        car_id: toInt(carId) ?? DEFAULT,
        driver_id: toInt(driverId) ?? DEFAULT,
        location_id: toInt(locationId) ?? DEFAULT,
        event_type: toInt(eventType) ?? DEFAULT,
        car_weight: toInt(carWeight),
        tow_angle: toFloat(towAngle),
        // alignment
        camber_front: toFloat(camberFront),
        camber_rear: toFloat(camberRear),
        toe_front: toFloat(toeFront),
        toe_rear: toFloat(toeRear),
        // ride height
        ride_height_front: toFloat(rideHeightFront),
        ride_height_rear: toFloat(rideHeightRear),
        // legacy ride_height omitted intentionally
        power_limit: toInt(powerLimit),
        // damping per corner
        fr_lsc: toInt(frLSC),
        fr_lsr: toInt(frLSR),
        fr_hsc: toInt(frHSC),
        fr_hsr: toInt(frHSR),
        fl_lsc: toInt(flLSC),
        fl_lsr: toInt(flLSR),
        fl_hsc: toInt(flHSC),
        fl_hsr: toInt(flHSR),
        rr_lsc: toInt(rrLSC),
        rr_lsr: toInt(rrLSR),
        rr_hsc: toInt(rrHSC),
        rr_hsr: toInt(rrHSR),
        rl_lsc: toInt(rlLSC),
        rl_lsr: toInt(rlLSR),
        rl_hsc: toInt(rlHSC),
        rl_hsr: toInt(rlHSR),
        torque_limit: toInt(torqueLimit),
        // pressures (existing columns)
        frw_pressure: toFloat(frPressure),
        flw_pressure: toFloat(flPressure),
        brw_pressure: toFloat(rrPressure),
        blw_pressure: toFloat(rlPressure),
        // tires
        fr_wear_depth: toFloat(frWearDepth),
        fl_wear_depth: toFloat(flWearDepth),
        rr_wear_depth: toFloat(rrWearDepth),
        rl_wear_depth: toFloat(rlWearDepth),
        fr_durometer: toFloat(frDurometer),
        fl_durometer: toFloat(flDurometer),
        rr_durometer: toFloat(rrDurometer),
        rl_durometer: toFloat(rlDurometer),
        // aero
        front_wing_on: frontWingOn ?? false,
        rear_wing_on: rearWingOn ?? false,
        front_wing_pitch: toFloat(frontWingPitch),
        rear_wing_pitch: toFloat(rearWingPitch),
        regen_on: regenOn ?? false,
        undertray_on: undertrayOn ?? false,
    };

    const specialtyData: any = {
      front_roll_spring_rate: toFloat(frontRollSpringRate),
      front_heave_spring_rate: toFloat(frontHeaveSpringRate),
      rear_roll_spring_rate: toFloat(rearRollSpringRate),
      rear_heave_spring_rate: toFloat(rearHeaveSpringRate),
      front_corner_spring_rate: toFloat(frontCornerSpringRate),
      rear_corner_spring_rate: toFloat(rearCornerSpringRate),
      front_arb_setting: frontArbSetting ?? null,
      rear_arb_setting: rearArbSetting ?? null,
    };

    // Merge and prune undefined keys to keep payload clean
    const mergedData = { ...baseData, ...specialtyData };
    Object.keys(mergedData).forEach((k) => {
      if (mergedData[k] === undefined) delete mergedData[k];
    });

    const newEvent = await prisma.event.create({
      data: mergedData,
    });

    return NextResponse.json({ event_id: newEvent.event_id }, { status: 201 });
  } catch (error) {
    console.error("Error creating event:", error);
    return NextResponse.json(
      { error: "Failed to create event" },
      { status: 500 }
    );
  }
}
