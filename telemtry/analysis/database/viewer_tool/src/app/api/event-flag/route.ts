import { NextRequest, NextResponse } from 'next/server';
import prisma from '@/lib/prisma/telemtry'; // Make sure you export PrismaClient instance here

export async function POST(req: NextRequest) {
  try {
    const activeEvent = await prisma.event.findFirst({
      where: { status: 2 },
      orderBy: { event_id: 'desc' },
    });

    if(!activeEvent) {
      return NextResponse.json({ message: 'No active event found' }, { status: 400 });
    }

    const body = await req.json();
    
    await prisma.classifier.create({
        data: {
            "type": "event_flag",
            "start_time": Date.now(),
            "event_id": activeEvent.event_id,
            "notes": body.eventFlag
        }
    })

    return NextResponse.json({ success: true }, { status: 200 });
  } catch (error) {
    console.error('Error updating event status:', error);
    return NextResponse.json({ error: 'Failed to update event status' }, { status: 500 });
  }
}
