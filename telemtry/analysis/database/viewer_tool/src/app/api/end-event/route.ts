import { NextRequest, NextResponse } from 'next/server';
import prisma from '@/lib/prisma/telemtry'; // Make sure you export PrismaClient instance here

export async function POST(req: NextRequest) {
  try {
    const now = Date.now();

    // Find the latest active event (status 1)
    const activeEvent = await prisma.event.findFirst({
      where: { status: 2 },
      orderBy: { event_id: 'desc' },
    });

    if (!activeEvent) {
      return NextResponse.json({ message: 'No active event found' }, { status: 204 });
    }

    const event_id = activeEvent.event_id;

    // Event is ending: need last packet_id
    const lastPacket = await prisma.packet.findFirst({
      orderBy: { packet_id: 'desc' },
    });

    const packet_end = lastPacket?.packet_id ?? 1;

    await prisma.event.update({
      where: { event_id },
      data: {
        end_time: now,
        status: 0,
        packet_end,
      },
    });

    return NextResponse.json({ success: true }, { status: 200 });
  } catch (error) {
    console.error('Error updating event status:', error);
    return NextResponse.json({ error: 'Failed to update event status' }, { status: 500 });
  }
}
