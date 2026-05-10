import { NextRequest, NextResponse } from 'next/server';
import prismaTelemtry from '@/lib/prisma/telemtry';
import {
  findLatestPacketId,
  normalizeCar,
  resolveCarFromCarId,
} from '@/lib/prisma/carPrisma';

function toBigInt(value: unknown): bigint | null {
  if (value === null || value === undefined || value === '') return null;
  try {
    return BigInt(value as string | number | bigint);
  } catch {
    return null;
  }
}

export async function POST(req: NextRequest) {
  try {
    const body = await req.json();
    const { day_id, status, start_time } = body;

    if (day_id === undefined || status === undefined) {
      return NextResponse.json({ error: 'Missing day_id or status' }, { status: 400 });
    }

    const dayId = Number(day_id);
    const nextStatus = Number(status);
    if (!Number.isFinite(dayId) || !Number.isFinite(nextStatus)) {
      return NextResponse.json({ error: 'Invalid day_id or status' }, { status: 400 });
    }

    const now = start_time ?? Date.now();
    const dayMeta = await prismaTelemtry.drive_day.findUnique({
      where: { day_id: dayId },
      select: {
        car_id: true,
        car: { select: { car_name: true } },
      },
    });

    if (!dayMeta) {
      return NextResponse.json({ error: 'Drive day not found' }, { status: 404 });
    }

    const car =
      normalizeCar(body.car) ??
      normalizeCar(dayMeta.car?.car_name) ??
      (await resolveCarFromCarId(dayMeta.car_id));

    if (nextStatus === 1) {
      await prismaTelemtry.drive_day.update({
        where: { day_id: dayId },
        data: {
          start_time: BigInt(now),
          status: 1,
        },
      });
    } else if (nextStatus === 0) {
      const lastPacketId = await findLatestPacketId(car);
      const fallbackPacketEnd = toBigInt(body.packet_end) ?? BigInt(0);
      const packet_end = lastPacketId ?? fallbackPacketEnd;

      await prismaTelemtry.drive_day.update({
        where: { day_id: dayId },
        data: {
          end_time: BigInt(now),
          status: 0,
          packet_end,
        },
      });
    } else {
      await prismaTelemtry.drive_day.update({
        where: { day_id: dayId },
        data: { status: nextStatus },
      });
    }

    return NextResponse.json({ success: true }, { status: 200 });
  } catch (error) {
    console.error('Error updating drive day status:', error);
    return NextResponse.json({ error: 'Failed to update drive day status' }, { status: 500 });
  }
}
