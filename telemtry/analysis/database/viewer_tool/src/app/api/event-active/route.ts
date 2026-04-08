import { NextResponse } from "next/server";
import prisma from "@/lib/prisma/telemtry";

export async function GET() {
  try {
    const latestEvent = await prisma.event.findFirst({
      orderBy: [{ creation_time: "desc" }, { event_id: "desc" }],
      select: {
        status: true,
        event_id: true,
        car_id: true,
        car: { select: { car_name: true } },
      },
    });

    const eventActive = latestEvent?.status === 2;

    return NextResponse.json({
      eventActive,
      eventId: latestEvent?.event_id ?? null,
      carId: latestEvent?.car_id ?? null,
      carName: latestEvent?.car?.car_name ?? null,
    });
  } catch (error) {
    return NextResponse.json(
      {
        eventActive: false,
        eventId: null,
        carId: null,
        carName: null,
      },
      { status: 200 }
    );
  }
}
