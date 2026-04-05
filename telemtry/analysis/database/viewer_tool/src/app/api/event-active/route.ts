import { NextResponse } from "next/server";
import prisma from "@/lib/prisma/telemtry";

export async function GET() {
  try {
    const latestEvent = await prisma.event.findFirst({
      orderBy: [{ creation_time: "desc" }, { event_id: "desc" }],
      select: { status: true, event_id: true },
    });

    const eventActive = latestEvent?.status === 2;

    return NextResponse.json({
      eventActive,
      eventId: latestEvent?.event_id ?? null,
    });
  } catch (error) {
    return NextResponse.json(
      {
        eventActive: false,
        eventId: null,
      },
      { status: 200 }
    );
  }
}
