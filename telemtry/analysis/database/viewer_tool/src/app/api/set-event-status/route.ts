import { NextRequest, NextResponse } from 'next/server';
import prisma from '@/lib/prisma/telemtry'; // Make sure you export PrismaClient instance here

export async function POST(req: NextRequest) {
  try {
    const body = await req.json();
    const { event_id, status, start_time } = body;

    if (event_id === undefined || status === undefined) {
      return NextResponse.json({ error: 'Missing event_id or status' }, { status: 400 });
    }

    const now = start_time ?? Date.now();

    if (status === 1) {
      // Event is starting
      await prisma.event.update({
        where: { event_id },
        data: {
          start_time: now,
          status: 1,
        },
      });
    } else if (status === 0) {
      // Event is ending: need last packet_id
      const lastPacket = await prisma.packet.findFirst({
        orderBy: { packet_id: 'desc' },
      });

      const packet_end = lastPacket?.packet_id ?? body.packet_end ?? 0;

      await prisma.event.update({
        where: { event_id },
        data: {
          end_time: now,
          status: 0,
          packet_end,
        },
      });
    } else {
      // Other statuses
      await prisma.event.update({
        where: { event_id },
        data: { status },
      });
    }

    // TODO: Publish to MQTT separately

    return NextResponse.json({ success: true }, { status: 200 });
  } catch (error) {
    console.error('Error updating event status:', error);
    return NextResponse.json({ error: 'Failed to update event status' }, { status: 500 });
  }
}
