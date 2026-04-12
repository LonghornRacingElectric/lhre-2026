import { NextRequest, NextResponse } from 'next/server';
import { Pool } from 'pg';

const pool = new Pool({
  host: process.env.DB_HOST || 'localhost',
  port: parseInt(process.env.DB_PORT || '5432'),
  database: process.env.DB_NAME || 'orion',
  user: process.env.DB_USER || 'analysis',
  password: process.env.DB_PASSWORD || '',
});

export async function POST(request: NextRequest) {
  let client;

  try {
    const body = await request.json();
    const { name, start_gate, points, sectors } = body;

    if (!name || typeof name !== 'string' || name.trim() === '') {
      return NextResponse.json(
        { error: 'Invalid request: name is required' },
        { status: 400 }
      );
    }

    if (
      !start_gate ||
      !Array.isArray(start_gate) ||
      start_gate.length !== 2 ||
      !Array.isArray(start_gate[0]) ||
      !Array.isArray(start_gate[1]) ||
      start_gate[0].length < 2 ||
      start_gate[1].length < 2
    ) {
      return NextResponse.json(
        { error: 'Invalid request: start_gate must be [[lat1, lon1], [lat2, lon2]]' },
        { status: 400 }
      );
    }

    const [[lat1, lon1], [lat2, lon2]] = start_gate;

    client = await pool.connect();

    const result = await client.query(
      `INSERT INTO public.track_mapping
         (name, created_at, start_gate_lat1, start_gate_lon1, start_gate_lat2, start_gate_lon2, points, sectors)
       VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
       RETURNING track_mapping_id`,
      [
        name.trim(),
        Date.now(),
        lat1,
        lon1,
        lat2,
        lon2,
        points ? JSON.stringify(points) : null,
        sectors ? JSON.stringify(sectors) : null,
      ]
    );

    return NextResponse.json(
      {
        success: true,
        track_mapping_id: result.rows[0].track_mapping_id,
        message: `Track mapping "${name.trim()}" saved successfully`,
      },
      { status: 200 }
    );
  } catch (error) {
    console.error('Error saving track mapping:', error);
    return NextResponse.json(
      { error: 'Failed to save track mapping' },
      { status: 500 }
    );
  } finally {
    if (client) {
      client.release();
    }
  }
}
