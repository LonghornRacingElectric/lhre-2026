import { NextResponse } from "next/server";
import prisma from "@/lib/prisma/telemtry";

export async function GET() {
  try {
    const latestDay = await prisma.drive_day.findFirst({
      orderBy: [{ creation_time: "desc" }, { day_id: "desc" }],
      select: { status: true, day_id: true },
    });

    const eventActive = latestDay?.status === 2;

    return NextResponse.json({
      eventActive,
      dayId: latestDay?.day_id ?? null,
    });
  } catch (error) {
    return NextResponse.json(
      {
        eventActive: false,
        dayId: null,
      },
      { status: 200 }
    );
  }
}
