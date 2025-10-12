export interface LapData {
  startTime: number;
  endTime: number;
  notes?: string;
}

export interface DriveDayState {
  dayId?: number;
  powerLimit?: string;
  drivingConditions?: string;
}

export interface NewEventState {
  eventId?: number;
  driverId?: number;
  locationId?: number;
  eventType?: number;
  carId?: number;
  carWeight?: string;
  towAngle?: string;
  camber?: string;
  rideHeight?: string;
  ackermanAdjustment?: string;
  powerLimit?: string;
  shockDampening?: string;
  torqueLimit?: string;
  frwPressure?: string;
  flwPressure?: string;
  brwPressure?: string;
  blwPressure?: string;
  frontWingOn?: boolean;
  rearWingOn?: boolean;
  regenOn?: boolean;
  undertrayOn?: boolean;
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