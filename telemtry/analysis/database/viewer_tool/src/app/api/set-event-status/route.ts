import { NextRequest, NextResponse } from 'next/server';
import prismaTelemtry from '@/lib/prisma/telemtry';
import {
  getCarPrisma,
  normalizeCar,
  resolveCarFromCarId,
} from '@/lib/prisma/carPrisma';

export async function POST(req: NextRequest) {
  try {
    const body = await req.json();
    const { event_id, status, start_time } = body;

    if (event_id === undefined || status === undefined) {
      return NextResponse.json({ error: 'Missing event_id or status' }, { status: 400 });
    }

    const now = start_time ?? Date.now();
    const eventMeta = await prismaTelemtry.event.findUnique({
      where: { event_id },
      select: {
        car_id: true,
        car: { select: { car_name: true } },
      },
    });
    const car =
      normalizeCar(body.car) ??
      normalizeCar(eventMeta?.car?.car_name) ??
      (await resolveCarFromCarId(eventMeta?.car_id));

    if (status === 1) {
      // Event is starting
      await prismaTelemtry.event.update({
        where: { event_id },
        data: {
          start_time: now,
          status: 1,
        },
      });
    } else if (status === 0) {
      // Event is ending: need last packet_id
      const carPrisma = getCarPrisma(car);
      const lastPacket = await carPrisma.packet.findFirst({
        orderBy: { packet_id: 'desc' },
        select: { packet_id: true },
      });
      const packet_end = lastPacket?.packet_id ?? body.packet_end ?? 0;

      await prismaTelemtry.event.update({
        where: { event_id },
        data: {
          end_time: now,
          status: 0,
          packet_end,
        },
      });
    } else {
      // Other statuses
      await prismaTelemtry.event.update({
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
