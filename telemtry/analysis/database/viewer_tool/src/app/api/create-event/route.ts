
import { NextRequest, NextResponse } from 'next/server';
import prisma from '@/lib/prisma/telemtry'; // adjust path to your prisma client

export async function POST(req: NextRequest) {
  try {
    const body = await req.json();

    // Set status to 2 (created and awaiting start)
    const status = 2;

    // Find the last event to determine packet_start
    const lastEvent = await prisma.event.findFirst({
      where: { status: 0 },
      orderBy: { event_id: 'desc' },
      select: { packet_end: true },
    });
    const packet_start = Number(lastEvent?.packet_end ?? 0) + 1;

    // Find the latest drive_day
    const currentDriveDay = await prisma.drive_day.findFirst({
      orderBy: { day_id: 'desc' },
      where: { date: new Date() },
      select: { day_id: true },
    });

    if (!currentDriveDay?.day_id) {
      return NextResponse.json({ error: 'No drive day found' }, { status: 500 });
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
      camber,
      rideHeight,
      ackermanAdjustment,
      powerLimit,
      shockDampening,
      torqueLimit,
      frwPressure,
      flwPressure,
      brwPressure,
      blwPressure,
      frontWingOn,
      rearWingOn,
      regenOn,
      undertrayOn,
    } = body;

    const newEvent = await prisma.event.create({
      data: {
        day_id: currentDriveDay.day_id,
        status,
        creation_time,
        start_time: creation_time,
        packet_start,
        car_id: carId ? parseInt(carId as any) : 9999,
        driver_id: driverId ? parseInt(driverId as any) : 9999,
        location_id: locationId ? parseInt(locationId as any) : 9999,
        event_type: eventType ? parseInt(eventType as any) : 9999,
        car_weight: carWeight ? parseInt(carWeight as any) : null,
        tow_angle: towAngle ? parseFloat(towAngle as any) : null,
        camber: camber ? parseFloat(camber as any) : null,
        ride_height: rideHeight ? parseFloat(rideHeight as any) : null,
        ackerman_adjustment: ackermanAdjustment ? parseFloat(ackermanAdjustment as any) : null,
        power_limit: powerLimit ? parseInt(powerLimit as any) : null,
        shock_dampening: shockDampening ? parseInt(shockDampening as any) : null,
        torque_limit: torqueLimit ? parseInt(torqueLimit as any) : null,
        frw_pressure: frwPressure ? parseFloat(frwPressure as any) : null,
        flw_pressure: flwPressure ? parseFloat(flwPressure as any) : null,
        brw_pressure: brwPressure ? parseFloat(brwPressure as any) : null,
        blw_pressure: blwPressure ? parseFloat(blwPressure as any) : null,
        front_wing_on: frontWingOn ?? false,
        rear_wing_on: rearWingOn ?? false,
        regen_on: regenOn ?? false,
        undertray_on: undertrayOn ?? false,
      }
      
    });

    return NextResponse.json({ event_id: newEvent.event_id }, { status: 201 });
  } catch (error) {
    console.error('Error creating event:', error);
    return NextResponse.json({ error: 'Failed to create event' }, { status: 500 });
  }
}
