// TypeScript interfaces matching the WebSocket JSON spec (v1.0)
// See: Dashboard WebSocket JSON Requirements document

export interface CanData {
    // Dynamics
    speed: number | null;           // Vehicle speed [TBD: MPH or m/s]
    power: number | null;           // Motor power in kW (-80 to +80, negative = regen)
    odometer: number | null;        // Total distance [TBD: miles or km]

    // Battery / Pack
    soc: number | null;             // State of charge (0-100%)
    temperature: number | null;     // Battery temp in °C

    // Telemetry
    signalStrength: number | null;  // 5G signal (0-4 bars)

    // Shutdown circuit: 16 booleans (true = OK, false = FAULT)
    shutdown: boolean[] | null;

    // Optional driver-thread additions. Not yet emitted by dashd; the
    // demo data hook provides synthetic values so the layout is testable.
    brakeBias?: number | null;      // % front bias, 0-100

    // Driver-armable flags. Source TBD — could come from VCU over CAN
    // (steering-wheel rotary/switch state) or off-car settings. Not yet
    // wired to a real producer; demo hook synthesizes values.
    tcLevel?: number | null;        // Traction control level (vehicle-defined range)
    tcEnabled?: boolean | null;     // TC armed?
    regenEnabled?: boolean | null;  // Regenerative braking enabled?

    // Pit-diagnostic fields. Not yet emitted by dashd (per-field BACKEND
    // TODOs in PitDiagnostic.tsx); demo hook synthesizes values so the
    // layout is testable.
    apps?: number | null;             // Accelerator pedal %, 0-100
    bpps?: number | null;             // Brake pedal %, 0-100
    brakePressureFront?: number | null; // psi
    brakePressureRear?: number | null;  // psi
    motorTemp?: number | null;        // °C
    inverterTemp?: number | null;     // °C
    coolantTemp?: number | null;      // °C
    cellTempAvg?: number | null;      // °C, pack-averaged cell temp
    cellTempMin?: number | null;      // °C
    hvVoltage?: number | null;        // V (FSAE EV TSV cap: 600 V DC)
    hvCurrent?: number | null;        // A
    lvVoltage?: number | null;        // V (GLV bus)
    lvCurrent?: number | null;        // A
    wheelSpeedFL?: number | null;     // same units as speed
    wheelSpeedFR?: number | null;
    wheelSpeedRL?: number | null;
    wheelSpeedRR?: number | null;
}

export interface MqttData {
    lapDelta: number | null;        // Time delta vs reference lap in seconds
    energyDelta: number | null;     // Energy delta vs target in Wh
    lapsRemaining: number | null;   // Estimated session laps remaining

    // Optional driver-thread additions. Not yet emitted by dashd; the
    // demo data hook provides synthetic values so the layout is testable.
    lapsRemainingEnergy?: number | null; // Energy-based laps remaining
    bestLapTime?: number | null;         // Seconds
    lastLapTime?: number | null;         // Seconds
    currentLapTime?: number | null;      // Seconds (live, ticking)
    lapDeltaRate?: number | null;        // d(lapDelta)/dt, s/s
}

export interface DashMessage {
    seq: number;
    can: CanData;
    mqtt: MqttData;
}

// Shutdown circuit index-to-name mapping (matches spec)
export const SHUTDOWN_NAMES: string[] = [
    "LV Master Switch",    // 0
    "Shutdown Fuse",       // 1
    "R-ESTOP",             // 2
    "BMS",                 // 3
    "IMD",                 // 4
    "Battery ACU HVIL",    // 5
    "L-ESTOP",             // 6
    "D-ESTOP",             // 7
    "Inertial Switch",     // 8
    "BOTS",                // 9
    "BSPD",                // 10
    "E-Meter HVIL",        // 11
    "MSD HVIL",            // 12
    "Battery HVIL",        // 13
    "Inverter HVIL",       // 14
    "TSMS",                // 15
];
