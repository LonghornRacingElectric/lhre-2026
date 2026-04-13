import { NextRequest, NextResponse } from 'next/server';
import prisma from '@/lib/prisma/telemtry';

export async function POST(req: NextRequest) {
  try {
    const activeDay = await prisma.drive_day.findFirst({
      where: { status: 2 },
      orderBy: { day_id: 'desc' },
    });

    if (!activeDay) {
      return NextResponse.json({ message: 'No active drive day found' }, { status: 400 });
    }

    const body = await req.json();

    await prisma.classifier.create({
      data: {
        type: "event_flag",
        start_time: BigInt(Date.now()),
        day_id: BigInt(activeDay.day_id),
        notes: body.eventFlag,
      },
    });

    return NextResponse.json({ success: true }, { status: 200 });
  } catch (error) {
    console.error('Error flagging event:', error);
    return NextResponse.json({ error: 'Failed to flag event' }, { status: 500 });
  }
}
