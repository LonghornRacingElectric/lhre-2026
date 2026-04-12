import { NextRequest, NextResponse } from 'next/server';
import prisma from '@/lib/prisma/telemtry';

export async function POST(req: NextRequest) {
  try {
    const body = await req.json();
    const { day_id, status, start_time } = body;

    if (day_id === undefined || status === undefined) {
      return NextResponse.json({ error: 'Missing day_id or status' }, { status: 400 });
    }

    const now = start_time ?? Date.now();

    if (status === 1) {
      await prisma.drive_day.update({
        where: { day_id },
        data: {
          start_time: BigInt(now),
          status: 1,
        },
      });
    } else if (status === 0) {
      const lastPacket = await prisma.packet.findFirst({
        orderBy: { packet_id: 'desc' },
      });

      const packet_end = lastPacket?.packet_id ?? body.packet_end ?? BigInt(0);

      await prisma.drive_day.update({
        where: { day_id },
        data: {
          end_time: BigInt(now),
          status: 0,
          packet_end,
        },
      });
    } else {
      await prisma.drive_day.update({
        where: { day_id },
        data: { status },
      });
    }

    return NextResponse.json({ success: true }, { status: 200 });
  } catch (error) {
    console.error('Error updating drive day status:', error);
    return NextResponse.json({ error: 'Failed to update drive day status' }, { status: 500 });
  }
}
