import { NextRequest, NextResponse } from 'next/server';
import pool from '@/lib/db-telemetry';

export async function POST(req: NextRequest) {
  if (!pool) {
    return NextResponse.json({ error: 'Database connection not configured' }, { status: 500 });
  }
  const client = await pool.connect();
  try {
    const body = await req.json();
    const { event_id, time } = body;

    if (!event_id || !time) {
      return NextResponse.json({ error: 'Missing event_id or time' }, { status: 400 });
    }

    // Get the event start time
    const eventRes = await client.query('SELECT start_time FROM event WHERE event_id = $1', [event_id]);
    const eventStartTime = eventRes.rows[0]?.start_time;

    if (!eventStartTime) {
      return NextResponse.json({ error: 'Event not found or not started' }, { status: 404 });
    }

    // Get the end time of the last lap for this event
    const lastLapRes = await client.query(
      'SELECT end_time FROM classifier WHERE event_id = $1 AND type = \'lap\' ORDER BY end_time DESC LIMIT 1',
      [event_id]
    );
    const lastLapEndTime = lastLapRes.rows[0]?.end_time;

    const lapStartTime = lastLapEndTime || eventStartTime;
    const lapEndTime = time;

    const query = `
      INSERT INTO classifier (event_id, type, start_time, end_time)
      VALUES ($1, 'lap', $2, $3);
    `;

    const values = [event_id, lapStartTime, lapEndTime];

    await client.query(query, values);

    return NextResponse.json({ success: true }, { status: 201 });
  } catch (error) {
    console.error('Error creating new lap:', error);
    return NextResponse.json({ error: 'Failed to create new lap' }, { status: 500 });
  } finally {
    client.release();
  }
}
