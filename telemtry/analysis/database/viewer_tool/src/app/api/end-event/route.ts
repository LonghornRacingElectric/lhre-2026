import { NextRequest, NextResponse } from 'next/server';
import prismaTelemtry from '@/lib/prisma/telemtry';
import {
  findLatestPacketId,
  normalizeCar,
  resolveCarFromCarId,
} from '@/lib/prisma/carPrisma';

export async function POST(req: NextRequest) {
  try {
    const body = await req.json().catch(() => ({}));
    const now = Date.now();

    // Find the latest active event (status 2)
    const activeEvent = await prismaTelemtry.event.findFirst({
      where: { status: 2 },
      orderBy: { event_id: 'desc' },
      select: {
        event_id: true,
        car_id: true,
        car: { select: { car_name: true } },
      },
    });

    if (!activeEvent) {
      return NextResponse.json({ message: 'No active event found' }, { status: 204 });
    }

    const event_id = activeEvent.event_id;
    const car =
      normalizeCar(body?.car) ??
      normalizeCar(activeEvent.car?.car_name) ??
      (await resolveCarFromCarId(activeEvent.car_id));

    // Event is ending: need last packet_id
    const lastPacketId = await findLatestPacketId(car);
    const packet_end = lastPacketId ?? 1;

    await prismaTelemtry.event.update({
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
