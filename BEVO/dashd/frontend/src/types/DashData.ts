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

    // Shutdown circuit: booleans (true = OK, false = FAULT).
    // dashd currently emits four legs from diagnostics_low.shutdown_legX.
    // See SHUTDOWN_NAMES for the matching labels.
    shutdown: boolean[] | null;

    // PRNDL state from VCU. "P" (Park) or "D" (Drive); null if not yet
    // received or value is outside the known enum.
    prndl: string | null;

    // HV contactor states from HVC 0x131. pos+neg both closed means the
    // HV system is up; precharge is transient during sequencing.
    posContactor: boolean | null;
    negContactor: boolean | null;
    prechargeContactor: boolean | null;

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
    cellTempMax?: number | null;      // °C, hottest cell from pack.cells_temps[]
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

// Shutdown circuit / safety-fault items, in the order dashd emits them.
// LEG 1–4 are the hardware shutdown legs from DiagnosticsLow.shutdown_legX;
// BMS / IMD are *_error flags inverted into shutdown convention. Electrical
// team owns the leg-to-physical-component mapping; the placeholder leg names
// stay until that comes back.
export const SHUTDOWN_NAMES: string[] = [
    "LEG 1",       // diagnostics_low.shutdown_leg1
    "LEG 2",       // diagnostics_low.shutdown_leg2
    "LEG 3",       // diagnostics_low.shutdown_leg3
    "LEG 4",       // diagnostics_low.shutdown_leg4
    "BMS",         // !diagnostics_low.bmb_comm_error
    "IMD",         // !diagnostics_low.imd_gnd_isolation_error
    "BSPD",        // diagnostics_low.shutdown_bspd_status
    "E-METER",     // diagnostics_low.shutdown_emeter_status
    "DUI TEMP 1",  // !diagnostics_low.temp_shutdown_1
    "DUI TEMP 2",  // !diagnostics_low.temp_shutdown_2
];
