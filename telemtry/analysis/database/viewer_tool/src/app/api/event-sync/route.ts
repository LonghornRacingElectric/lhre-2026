import { NextRequest, NextResponse } from "next/server";
import { EventEmitter } from "events";
import { AppState, DriveDayState, NewEventState } from "@/lib/types";
import prisma from "@/lib/prisma/telemtry";

const emitter = new EventEmitter();
emitter.setMaxListeners(100);

let appState: AppState = {}; // In-memory state
let initialized = false;

async function initAppState() {
  try {
    let currentPage: string | undefined = undefined;

    // Fetch the current drive day
    const currentDriveDay = await prisma.drive_day.findFirst({
      orderBy: { day_id: "desc" },
      where: { date: new Date() },
    });

    let driveDay: DriveDayState | undefined;
    if (currentDriveDay) {
      driveDay = {
        dayId: currentDriveDay.day_id,
        powerLimit: currentDriveDay.power_limit ?? undefined,
        airTemperature: currentDriveDay.air_temperature ?? undefined,
        relativeHumidity: currentDriveDay.relative_humidity ?? undefined,
        trackTemperature: currentDriveDay.track_temperature ?? undefined,
      };
      currentPage = "/event/new";
    }

    // Fetch the currently active event (status = 2)
    const activeEvent = await prisma.event.findFirst({
      where: { status: 2 },
      orderBy: { creation_time: "desc" },
    });

    let newEvent: NewEventState | undefined;
    if (currentDriveDay && activeEvent) {
      newEvent = {
        eventId: activeEvent.event_id,
        driverId: activeEvent.driver_id,
        locationId: activeEvent.location_id,
        eventType: activeEvent.event_type,
        carId: activeEvent.car_id,
        carWeight: activeEvent.car_weight?.toString(),
        towAngle: activeEvent.tow_angle?.toString(),
        camberFront: activeEvent.camber_front?.toString(),
        camberRear: activeEvent.camber_rear?.toString(),
        toeFront: activeEvent.toe_front?.toString(),
        toeRear: activeEvent.toe_rear?.toString(),
        rideHeightFront: activeEvent.ride_height_front?.toString(),
        rideHeightRear: activeEvent.ride_height_rear?.toString(),
        powerLimit: activeEvent.power_limit?.toString(),
        torqueLimit: activeEvent.torque_limit?.toString(),
        frPressure: activeEvent.frw_pressure?.toString(),
        flPressure: activeEvent.flw_pressure?.toString(),
        rrPressure: activeEvent.brw_pressure?.toString(),
        rlPressure: activeEvent.blw_pressure?.toString(),
        frontWingOn: false, // default, or fetch from DB if stored
        rearWingOn: false,
        regenOn: false,
        undertrayOn: false,
      };

      currentPage = "/event/in-progress";
    }

    // Set app state
    appState = {
      ...appState,
      newEvent,
      driveDay,
      lastUpdatedBy: undefined,
      currentPage: currentPage,
    };
  } catch (e) {
    console.error("Database not available, running in offline mode.");
  }

  initialized = true;
}

export async function GET() {
  if (!initialized) await initAppState();
  let listener: (data: AppState) => void;
  const stream = new ReadableStream({
    start(controller) {
      listener = (data: AppState) => {
        controller.enqueue(`data: ${JSON.stringify(data)}\n\n`);
      };
      emitter.on("update", listener);
      controller.enqueue(`data: ${JSON.stringify(appState)}\n\n`);
    },
    cancel() {
      emitter.off("update", listener);
    },
  });

  return new Response(stream, {
    headers: {
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache",
      Connection: "keep-alive",
    },
  });
}

export async function POST(request: NextRequest) {
  try {
    const body = await request.json();
    appState = { ...appState, ...body }; // Merge the new state
    emitter.emit("update", appState);
    return NextResponse.json({ success: true });
  } catch (error) {
    return NextResponse.json({ error: "Invalid request" }, { status: 400 });
  }
}
