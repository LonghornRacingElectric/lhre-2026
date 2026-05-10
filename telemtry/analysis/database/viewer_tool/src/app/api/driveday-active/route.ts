import { NextResponse } from "next/server";
import prisma from "@/lib/prisma/telemtry";

export async function GET() {
  try {
    const latestDay = await prisma.drive_day.findFirst({
      orderBy: { day_id: "desc" },
      select: {
        status: true,
        day_id: true,
        car_id: true,
        car: { select: { car_name: true } },
      },
    });

    const eventActive = latestDay?.status === 2;

    return NextResponse.json({
      eventActive,
      dayId: latestDay?.day_id ?? null,
      eventId: latestDay?.day_id ?? null,
      carId: latestDay?.car_id ?? null,
      carName: latestDay?.car?.car_name ?? null,
    });
  } catch (error) {
    return NextResponse.json(
      {
        eventActive: false,
        dayId: null,
        eventId: null,
        carId: null,
        carName: null,
      },
      { status: 200 }
    );
  }
}
