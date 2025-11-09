import { NextRequest, NextResponse } from 'next/server';
import prisma from '@/lib/prisma/telemtry';

export async function POST(req: NextRequest) {
  try {
    const body = await req.json();
    const { power_limit, air_temperature, relative_humidity, track_temperature } = body;

    const today = new Date();

    const newDay = await prisma.drive_day.create({
      data: {
        date: today,
        power_limit,
        air_temperature,
        relative_humidity,
        track_temperature,
      },
      select: { day_id: true }, // only return the ID
    });

    return NextResponse.json({ day_id: newDay.day_id }, { status: 201 });
  } catch (error) {
    console.error('Error creating new drive day:', error);
    return NextResponse.json({ error: 'Failed to create new drive day' }, { status: 500 });
  } finally {
    await prisma.$disconnect();
  }
}
