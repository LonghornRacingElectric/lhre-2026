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

    // Find the latest active drive day (status 2)
    const activeDay = await prismaTelemtry.drive_day.findFirst({
      where: { status: 2 },
      orderBy: { day_id: 'desc' },
      select: {
        day_id: true,
        car_id: true,
        car: { select: { car_name: true } },
      },
    });

    if (!activeDay) {
      return NextResponse.json({ message: 'No active drive day found' }, { status: 204 });
    }

    const car =
      normalizeCar(body?.car) ??
      normalizeCar(activeDay.car?.car_name) ??
      (await resolveCarFromCarId(activeDay.car_id));

    // Capture packet end from the selected car stream when possible.
    const lastPacketId = await findLatestPacketId(car);
    const packet_end = lastPacketId ?? BigInt(1);

    await prismaTelemtry.drive_day.update({
      where: { day_id: activeDay.day_id },
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
