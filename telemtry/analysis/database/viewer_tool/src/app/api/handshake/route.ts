import { NextRequest, NextResponse } from 'next/server';
import pool from '@/lib/db-telemetry';

export async function GET(req: NextRequest) {
  if (!pool) {
    return NextResponse.json({ error: 'Database connection not configured' }, { status: 500 });
  }
  const client = await pool.connect();
  try {
    const time = Date.now();

    const lastPacketRes = await client.query('SELECT packet_id FROM packet ORDER BY packet_id DESC LIMIT 1');
    const last_packet = lastPacketRes.rows[0]?.packet_id || 0;

    return NextResponse.json({ time, last_packet });
  } catch (e) {
    console.error('Error in handshake:', e);
    const error = e as { code?: string };
    // If the packet table does not exist, we should still return a time.
    if (error.code === '42P01') { // undefined_table
        return NextResponse.json({ time: Date.now(), last_packet: 0 });
    }
    return NextResponse.json({ error: 'Failed to perform handshake' }, { status: 500 });
  } finally {
    client.release();
  }
}
