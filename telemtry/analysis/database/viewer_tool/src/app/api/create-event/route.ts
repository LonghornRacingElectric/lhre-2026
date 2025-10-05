
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
    const lastDriveDay = await prisma.drive_day.findFirst({
      orderBy: { day_id: 'desc' },
      select: { day_id: true },
    });

    if (!lastDriveDay?.day_id) {
      return NextResponse.json({ error: 'No drive day found' }, { status: 500 });
    }

    const creation_time = Date.now();

    const { 
      driver_id, location_id, event_type, car_id, car_weight, tow_angle, camber,
      ride_height, ackerman_adjustment, power_limit, shock_dampening, torque_limit,
      frw_pressure, flw_pressure, brw_pressure, blw_pressure
    } = body;

    const newEvent = await prisma.event.create({
      data: {
        day_id: lastDriveDay.day_id,
        status,
        creation_time,
        packet_start,
        car_id: car_id ? parseInt(car_id) : 9999,
        driver_id: driver_id ? parseInt(driver_id) : 9999,
        location_id: location_id ? parseInt(location_id) : 9999,
        event_type: event_type ? parseInt(event_type) : 9999,
        car_weight: car_weight ? parseInt(car_weight) : null,
        tow_angle: tow_angle ? parseFloat(tow_angle) : null,
        camber: camber ? parseFloat(camber) : null,
        ride_height: ride_height ? parseFloat(ride_height) : null,
        ackerman_adjustment: ackerman_adjustment ? parseFloat(ackerman_adjustment) : null,
        power_limit: power_limit ? parseInt(power_limit) : null,
        shock_dampening: shock_dampening ? parseInt(shock_dampening) : null,
        torque_limit: torque_limit ? parseInt(torque_limit) : null,
        frw_pressure: frw_pressure ? parseFloat(frw_pressure) : null,
        flw_pressure: flw_pressure ? parseFloat(flw_pressure) : null,
        brw_pressure: brw_pressure ? parseFloat(brw_pressure) : null,
        blw_pressure: blw_pressure ? parseFloat(blw_pressure) : null,
      }
    });

    // TODO: Publish event_id to MQTT

    return NextResponse.json({ event_id: newEvent.event_id }, { status: 201 });
  } catch (error) {
    console.error('Error creating event:', error);
    return NextResponse.json({ error: 'Failed to create event' }, { status: 500 });
  }
}
