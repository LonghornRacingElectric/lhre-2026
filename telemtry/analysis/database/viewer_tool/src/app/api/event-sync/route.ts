import { NextRequest, NextResponse } from "next/server";
import { EventEmitter } from "events";
import { AppState, DriveDayState } from "@/lib/types";
import prisma from "@/lib/prisma/telemtry";

const emitter = new EventEmitter();
emitter.setMaxListeners(100);

let appState: AppState = {}; // In-memory state
let initialized = false;

async function initAppState() {
  try {
    // Fetch the most recent drive day
    const activeDay = await prisma.drive_day.findFirst({
      orderBy: { day_id: "desc" },
    });

    let driveDay: DriveDayState | undefined;
    let currentPage: string | undefined;

    if (activeDay) {
      driveDay = {
        dayId: activeDay.day_id,
        trackName: activeDay.track_name ?? undefined,
        weather: activeDay.weather ?? undefined,
        windSpeed: activeDay.wind_speed?.toString() ?? undefined,
        powerLimit: activeDay.power_limit ?? undefined,
        airTemperature: activeDay.air_temperature ?? undefined,
        relativeHumidity: activeDay.relative_humidity ?? undefined,
        trackTemperature: activeDay.track_temperature ?? undefined,
        driverId: activeDay.driver_id ?? undefined,
        carId: activeDay.car_id ?? undefined,
        eventType: activeDay.event_type ?? undefined,
        carWeight: activeDay.car_weight?.toString() ?? undefined,
        towAngle: activeDay.tow_angle?.toString() ?? undefined,
        camberFront: activeDay.camber_front?.toString() ?? undefined,
        camberRear: activeDay.camber_rear?.toString() ?? undefined,
        toeFront: activeDay.toe_front?.toString() ?? undefined,
        toeRear: activeDay.toe_rear?.toString() ?? undefined,
        rideHeightFront: activeDay.ride_height_front?.toString() ?? undefined,
        rideHeightRear: activeDay.ride_height_rear?.toString() ?? undefined,
        torqueLimit: activeDay.torque_limit?.toString() ?? undefined,
        frLSC: activeDay.fr_lsc?.toString() ?? undefined,
        frLSR: activeDay.fr_lsr?.toString() ?? undefined,
        frHSC: activeDay.fr_hsc?.toString() ?? undefined,
        frHSR: activeDay.fr_hsr?.toString() ?? undefined,
        flLSC: activeDay.fl_lsc?.toString() ?? undefined,
        flLSR: activeDay.fl_lsr?.toString() ?? undefined,
        flHSC: activeDay.fl_hsc?.toString() ?? undefined,
        flHSR: activeDay.fl_hsr?.toString() ?? undefined,
        rrLSC: activeDay.rr_lsc?.toString() ?? undefined,
        rrLSR: activeDay.rr_lsr?.toString() ?? undefined,
        rrHSC: activeDay.rr_hsc?.toString() ?? undefined,
        rrHSR: activeDay.rr_hsr?.toString() ?? undefined,
        rlLSC: activeDay.rl_lsc?.toString() ?? undefined,
        rlLSR: activeDay.rl_lsr?.toString() ?? undefined,
        rlHSC: activeDay.rl_hsc?.toString() ?? undefined,
        rlHSR: activeDay.rl_hsr?.toString() ?? undefined,
        frPressure: activeDay.frw_pressure?.toString() ?? undefined,
        flPressure: activeDay.flw_pressure?.toString() ?? undefined,
        rrPressure: activeDay.brw_pressure?.toString() ?? undefined,
        rlPressure: activeDay.blw_pressure?.toString() ?? undefined,
        frHotPressure: activeDay.frw_hot_pressure?.toString() ?? undefined,
        flHotPressure: activeDay.flw_hot_pressure?.toString() ?? undefined,
        rrHotPressure: activeDay.brw_hot_pressure?.toString() ?? undefined,
        rlHotPressure: activeDay.blw_hot_pressure?.toString() ?? undefined,
        frWearDepth: activeDay.fr_wear_depth?.toString() ?? undefined,
        flWearDepth: activeDay.fl_wear_depth?.toString() ?? undefined,
        rrWearDepth: activeDay.rr_wear_depth?.toString() ?? undefined,
        rlWearDepth: activeDay.rl_wear_depth?.toString() ?? undefined,
        frDurometer: activeDay.fr_durometer?.toString() ?? undefined,
        flDurometer: activeDay.fl_durometer?.toString() ?? undefined,
        rrDurometer: activeDay.rr_durometer?.toString() ?? undefined,
        rlDurometer: activeDay.rl_durometer?.toString() ?? undefined,
        frontWingOn: activeDay.front_wing_on ?? false,
        rearWingOn: activeDay.rear_wing_on ?? false,
        frontWingPitch: activeDay.front_wing_pitch?.toString() ?? undefined,
        rearWingPitch: activeDay.rear_wing_pitch?.toString() ?? undefined,
        regenOn: activeDay.regen_on ?? false,
        undertrayOn: activeDay.undertray_on ?? false,
        frontRollSpringRate: activeDay.front_roll_spring_rate?.toString() ?? undefined,
        frontHeaveSpringRate: activeDay.front_heave_spring_rate?.toString() ?? undefined,
        rearRollSpringRate: activeDay.rear_roll_spring_rate?.toString() ?? undefined,
        rearHeaveSpringRate: activeDay.rear_heave_spring_rate?.toString() ?? undefined,
        frontCornerSpringRate: activeDay.front_corner_spring_rate?.toString() ?? undefined,
        rearCornerSpringRate: activeDay.rear_corner_spring_rate?.toString() ?? undefined,
        frontArbSetting: activeDay.front_arb_setting ?? undefined,
        rearArbSetting: activeDay.rear_arb_setting ?? undefined,
      };

      currentPage = activeDay.status === 2 ? "/driveday-active" : "/driveday";
    }

    appState = {
      ...appState,
      driveDay,
      lastUpdatedBy: undefined,
      currentPage,
    };
  } catch (e) {
    console.error("Database not available, running in offline mode.", e);
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
    appState = { ...appState, ...body };
    emitter.emit("update", appState);
    return NextResponse.json({ success: true });
  } catch (error) {
    return NextResponse.json({ error: "Invalid request" }, { status: 400 });
  }
}
