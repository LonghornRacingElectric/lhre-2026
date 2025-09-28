import { NextRequest, NextResponse } from 'next/server';
import pool from '@/lib/db-telemetry';

export async function POST(req: NextRequest) {
  const client = await pool.connect();
  try {
    const body = await req.json();
    const { event_id, status, start_time } = body;

    if (event_id === undefined || status === undefined) {
      return NextResponse.json({ error: 'Missing event_id or status' }, { status: 400 });
    }

    const now = start_time ? start_time : Date.now();
    let query;
    let values;

    if (status === 1) { // Event is starting
      query = 'UPDATE event SET start_time = $1, status = 1 WHERE event_id = $2';
      values = [now, event_id];
    } else if (status === 0) { // Event is ending
      // Get the last packet_id from the packet table
      const lastPacketRes = await client.query('SELECT packet_id FROM packet ORDER BY packet_id DESC LIMIT 1');
      const packet_end = lastPacketRes.rows[0]?.packet_id || body.packet_end || 0;
      query = 'UPDATE event SET end_time = $1, status = 0, packet_end = $2 WHERE event_id = $3';
      values = [now, packet_end, event_id];
    } else {
      // For other statuses, just update the status
      query = 'UPDATE event SET status = $1 WHERE event_id = $2';
      values = [status, event_id];
    }

    await client.query(query, values);

    // In the flask app, this published to MQTT. That will be a separate step.
    // with MQTTHandler(f'flask_app_{uuid.uuid4()}') as mqtt:
    //   mqtt.publish('config/flask', 'end_event')

    return NextResponse.json({ success: true }, { status: 200 });
  } catch (error) {
    console.error('Error updating event status:', error);
    return NextResponse.json({ error: 'Failed to update event status' }, { status: 500 });
  } finally {
    client.release();
  }
}
