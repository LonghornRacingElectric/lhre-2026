import { NextRequest, NextResponse } from 'next/server';
import { Pool } from 'pg';

const pool = new Pool({
  host: process.env.DB_HOST || 'localhost',
  port: parseInt(process.env.DB_PORT || '5432'),
  database: process.env.DB_NAME || 'angelique',
  user: process.env.DB_USER || 'analysis',
  password: process.env.DB_PASSWORD || '',
});

export async function POST(request: NextRequest) {
  let client;
  
  try {
    const body = await request.json();
    const { event_id, points } = body;

    if (!event_id || !Array.isArray(points) || points.length === 0) {
      return NextResponse.json(
        { error: 'Invalid request: event_id and points array required' },
        { status: 400 }
      );
    }

    client = await pool.connect();
    let saved_count = 0;
    
    for (const point of points) {
      const { latitude, longitude, timestamp_ms } = point;
      
      await client.query(
        `INSERT INTO public.track_point (event_id, latitude, longitude, timestamp_ms)
         VALUES ($1, $2, $3, $4)`,
        [event_id, latitude, longitude, timestamp_ms]
      );
      saved_count++;
    }

    return NextResponse.json(
      { 
        success: true, 
        saved_count,
        message: `Successfully saved ${saved_count} points for event ${event_id}`
      },
      { status: 200 }
    );
  } catch (error) {
    console.error('Error saving track points:', error);
    const errorMessage = error instanceof Error ? error.message : 'Failed to save track points';
    return NextResponse.json(
      { 
        error: errorMessage,
        details: error instanceof Error ? error.stack : 'Unknown error'
      },
      { status: 500 }
    );
  } finally {
    if (client) {
      client.release();
    }
  }
}
