import { NextRequest, NextResponse } from 'next/server';
import pool from '@/lib/db-telemetry';

export async function POST(req: NextRequest) {
  const client = await pool.connect();
  try {
    const body = await req.json();

    // Set status to 2 (created and awaiting start)
    const status = 2;

    // Find the last event to determine the packet_start
    const lastEventRes = await client.query(
      'SELECT packet_end FROM event WHERE status = 0 ORDER BY event_id DESC LIMIT 1'
    );
    const lastPacket = lastEventRes.rows[0]?.packet_end || 0;
    const packet_start = lastPacket + 1;

    // Find the latest drive_day
    const lastDriveDayRes = await client.query(
      'SELECT day_id FROM drive_day ORDER BY day_id DESC LIMIT 1'
    );
    const day_id = lastDriveDayRes.rows[0]?.day_id;

    if (!day_id) {
      return NextResponse.json({ error: 'No drive day found' }, { status: 500 });
    }

    const creation_time = Date.now();

    const { 
        driver_id, 
        location_id, 
        event_type, 
        car_id, 
        car_weight, 
        tow_angle, 
        camber, 
        ride_height, 
        ackerman_adjustment, 
        power_limit, 
        shock_dampening, 
        torque_limit, 
        frw_pressure, 
        flw_pressure, 
        brw_pressure, 
        blw_pressure 
    } = body;

    const query = `
      INSERT INTO event (day_id, status, creation_time, packet_start, car_id, driver_id, location_id, event_type, car_weight, tow_angle, camber, ride_height, ackerman_adjustment, power_limit, shock_dampening, torque_limit, frw_pressure, flw_pressure, brw_pressure, blw_pressure)
      VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18, $19, $20)
      RETURNING event_id;
    `;

    const values = [
        day_id, 
        status, 
        creation_time, 
        packet_start, 
        parseInt(car_id),
        parseInt(driver_id),
        parseInt(location_id),
        parseInt(event_type),
        car_weight ? parseInt(car_weight) : null,
        tow_angle ? parseFloat(tow_angle) : null,
        camber ? parseFloat(camber) : null,
        ride_height ? parseFloat(ride_height) : null,
        ackerman_adjustment ? parseFloat(ackerman_adjustment) : null,
        power_limit ? parseInt(power_limit) : null,
        shock_dampening ? parseInt(shock_dampening) : null,
        torque_limit ? parseInt(torque_limit) : null,
        frw_pressure ? parseFloat(frw_pressure) : null,
        flw_pressure ? parseFloat(flw_pressure) : null,
        brw_pressure ? parseFloat(brw_pressure) : null,
        blw_pressure ? parseFloat(blw_pressure) : null,
    ];

    const result = await client.query(query, values);
    const newEventId = result.rows[0].event_id;

    // TODO: Publish event_id to MQTT

    return NextResponse.json({ event_id: newEventId }, { status: 201 });
  } catch (error) {
    console.error('Error creating event:', error);
    return NextResponse.json({ error: 'Failed to create event' }, { status: 500 });
  } finally {
    client.release();
  }
}