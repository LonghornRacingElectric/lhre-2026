import { NextRequest, NextResponse } from 'next/server';

export async function POST(req: NextRequest) {
  try {
    const body = await req.json();
    console.log('Received accel data:', body);
    return NextResponse.json({ success: true });
  } catch (error) {
    console.error('Error receiving accel data:', error);
    return NextResponse.json({ error: 'Invalid request' }, { status: 400 });
  }
}
