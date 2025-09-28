import { NextRequest, NextResponse } from 'next/server';
import pool from '@/lib/db-telemetry';

export async function POST(req: NextRequest) {
  const client = await pool.connect();
  try {
    const body = await req.json();
    const { power_limit, conditions } = body;

    const today = new Date();

    const query = `
      INSERT INTO drive_day (date, power_limit, conditions)
      VALUES ($1, $2, $3)
      RETURNING day_id;
    `;

    const values = [today, power_limit, conditions];

    const result = await client.query(query, values);
    const newDayId = result.rows[0].day_id;

    return NextResponse.json({ day_id: newDayId }, { status: 201 });
  } catch (error) {
    console.error('Error creating new drive day:', error);
    return NextResponse.json({ error: 'Failed to create new drive day' }, { status: 500 });
  } finally {
    client.release();
  }
}
