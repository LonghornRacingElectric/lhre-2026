import { NextRequest, NextResponse } from 'next/server';

export async function POST(req: NextRequest) {
  try {
    const body = await req.json();
    const { image } = body;

    if (!image) {
      return NextResponse.json({ error: 'Missing image data' }, { status: 400 });
    }

    // Forward the image data to the event-sync endpoint
    await fetch(new URL('/api/event-sync', req.url), {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ liveImage: image }),
    });

    return NextResponse.json({ success: true });
  } catch (error) {
    console.error('Error updating live image:', error);
    return NextResponse.json({ error: 'Failed to update live image' }, { status: 500 });
  }
}
