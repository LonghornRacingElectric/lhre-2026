import { NextRequest, NextResponse } from 'next/server';
import prisma from '@/lib/prisma/telemtry';

export async function POST(req: NextRequest) {
  try {
    const now = Date.now();

    // Find the latest active drive day (status 2)
    const activeDay = await prisma.drive_day.findFirst({
      where: { status: 2 },
      orderBy: { day_id: 'desc' },
    });

    if (!activeDay) {
      return NextResponse.json({ message: 'No active drive day found' }, { status: 204 });
    }

    const day_id = activeDay.day_id;

    const lastPacket = await prisma.packet.findFirst({
      orderBy: { packet_id: 'desc' },
    });

    const packet_end = lastPacket?.packet_id ?? BigInt(1);

    await prisma.drive_day.update({
      where: { day_id },
      data: {
        end_time: BigInt(now),
        status: 0,
        packet_end,
      },
    });

    return NextResponse.json({ success: true }, { status: 200 });
  } catch (error) {
    console.error('Error ending drive day:', error);
    return NextResponse.json({ error: 'Failed to end drive day' }, { status: 500 });
  }
}
