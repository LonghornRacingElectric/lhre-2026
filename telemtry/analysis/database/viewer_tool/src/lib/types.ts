export interface LapData {
  startTime: number;
  endTime: number;
  notes?: string;
}

export interface DriveDayState {
  dayId?: number;
  powerLimit?: number;
  airTemperature?: number;
  relativeHumidity?: number;
  trackTemperature?: number;
  trackName?: string;
}

export interface NewEventState {
  eventId?: number;
  driverId?: number;
  locationId?: number;
  eventType?: number;
  carId?: number;
  carWeight?: string;
  towAngle?: string;
  // Alignment
  camberFront?: string;
  camberRear?: string;
  toeFront?: string;
  toeRear?: string;
  // Ride height
  rideHeightFront?: string;
  rideHeightRear?: string;
  powerLimit?: string;
  // Shock damping per corner (LSC/LSR/HSC/HSR)
  frLSC?: string; frLSR?: string; frHSC?: string; frHSR?: string;
  flLSC?: string; flLSR?: string; flHSC?: string; flHSR?: string;
  rrLSC?: string; rrLSR?: string; rrHSC?: string; rrHSR?: string;
  rlLSC?: string; rlLSR?: string; rlHSC?: string; rlHSR?: string;
  torqueLimit?: string;
  // Tire cold pressures (psi)
  frPressure?: string;
  flPressure?: string;
  rrPressure?: string;
  rlPressure?: string;
  // Tire wear depth and durometer
  frWearDepth?: string; flWearDepth?: string; rrWearDepth?: string; rlWearDepth?: string;
  frDurometer?: string; flDurometer?: string; rrDurometer?: string; rlDurometer?: string;
  frontWingOn?: boolean;
  rearWingOn?: boolean;
  frontWingPitch?: string;
  rearWingPitch?: string;
  regenOn?: boolean;
  undertrayOn?: boolean;
  // Optional specialty fields
  // Angelique only
  frontRollSpringRate?: string;
  frontHeaveSpringRate?: string;
  rearRollSpringRate?: string;
  rearHeaveSpringRate?: string;
  // 2026 car only (per axle)
  frontCornerSpringRate?: string;
  rearCornerSpringRate?: string;
  frontArbSetting?: string; // low | medium | stiff
  rearArbSetting?: string;  // low | medium | stiff
}

export interface EventTrackerState {
  isTimerRunning?: boolean;
  timerStartTime?: number;
  timerBaseTime?: number;
  isTurning?: boolean;
  turnStartTime?: number;
  isAccel?: boolean;
  accelStartTime?: number;
  turns?: LapData[];
  accels?: LapData[];
  laps?: number[];
}

export interface AppState {
  lastUpdatedBy?: string;
  currentPage?: string;
  driveDay?: DriveDayState;
  newEvent?: NewEventState;
  eventTracker?: EventTrackerState;
}