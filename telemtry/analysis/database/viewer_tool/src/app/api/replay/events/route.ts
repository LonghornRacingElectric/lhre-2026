import { NextResponse } from 'next/server';
import prisma from '@/lib/prisma/telemtry';

export const runtime = 'nodejs';
export const dynamic = 'force-dynamic';

export async function GET() {
  try {
    const rows = await prisma.event.findMany({
      orderBy: { event_id: 'desc' },
      take: 200,
      include: {
        drive_day: { select: { date: true } },
        driver: { select: { driver_name: true } },
        location: { select: { area: true, track: true } },
        eventType: { select: { event_type: true } },
        car: { select: { car_name: true } },
      },
    });

    const events = rows.map((e) => ({
      event_id: e.event_id,
      status: e.status,
      creation_time: e.creation_time.toString(),
      start_time: e.start_time != null ? e.start_time.toString() : null,
      end_time: e.end_time != null ? e.end_time.toString() : null,
      packet_start: e.packet_start != null ? e.packet_start.toString() : null,
      packet_end: e.packet_end != null ? e.packet_end.toString() : null,
      day_date: e.drive_day?.date != null ? e.drive_day.date.toISOString() : null,
      driver_name: e.driver?.driver_name ?? null,
      area: e.location?.area ?? null,
      track: e.location?.track ?? null,
      event_type: e.eventType?.event_type ?? null,
      car_name: e.car?.car_name ?? null,
    }));

    return NextResponse.json({ events }, { status: 200 });
  } catch (e) {
    console.error('Failed to list replay events', e);
    return NextResponse.json({ error: 'Failed to list replay events' }, { status: 500 });
  }
}
