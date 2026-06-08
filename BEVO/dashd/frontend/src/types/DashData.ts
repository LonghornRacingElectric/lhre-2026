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

    // Active VCU event mode — which params table the firmware is running.
    // Source: Controls.event_mode (byte 6 of 0x1C7 VCU State). See
    // EVENT_MODE_LABELS for the enum->label mapping.
    eventMode?: number | null;

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

    // Pack cell-voltage aggregates from pack.cells_v[]. cellVSpread (max-min)
    // is the pack-imbalance health metric watched in the pit.
    cellVMax?: number | null;         // V, highest cell
    cellVMin?: number | null;         // V, lowest cell
    cellVSpread?: number | null;      // V, max - min

    // Running cumulative energy from the VCU's 0x1C9 Energy Estimate (Wh).
    // VCU is the source of truth for energy — no client-side integration.
    vcuNetEnergyWh?: number | null;     // net = drive - regen returned
    vcuRegenEnergyWh?: number | null;   // cumulative regen returned
}

export interface MqttData {
    lapDelta: number | null;        // Time delta vs reference lap in seconds
    energyDelta: number | null;     // Energy delta vs target in Wh
    lapsRemaining: number | null;   // Estimated session laps remaining

    // Endurance pacing signals from the trackside "Dash" sender tab
    // (BEVO/dashd/MQTT_CONTRACT.md). targetPower is the live power budget the
    // strategist dials in; lapTrigger is a monotonic counter bumped each lap.
    // The dash integrates CAN power on-board against targetPower to drive the
    // energy-budget bar, and fires the lap card when lapTrigger increases.
    targetPower?: number | null;    // kW — live target power budget (held last-known)
    targetPowerStale?: boolean | null; // true when the held targetPower is past staleness
    lapTrigger?: number | null;     // monotonic lap counter (rising edge = new lap)
    lapCardMs?: number | null;      // ms the full-screen lap card stays up (website-set)

    // Optional driver-thread additions. Not yet emitted by dashd; the
    // demo data hook provides synthetic values so the layout is testable.
    lapsRemainingEnergy?: number | null; // Energy-based laps remaining
    bestLapTime?: number | null;         // Seconds
    lastLapTime?: number | null;         // Seconds
    currentLapTime?: number | null;      // Seconds (live, ticking)
    lapDeltaRate?: number | null;        // d(lapDelta)/dt, s/s
}

// Endurance pacing, computed authoritatively on-car by dashd (see
// useEnergyPacing + BEVO/dashd/main.rs PacingData). The frontend just displays
// these — integrating here would reset the lap on a chromium reload.
export interface PacingData {
    lapEnergyWh: number;             // net energy used this lap (Wh)
    budgetDeltaWh: number | null;    // used - budget; >0 over (red), <0 under (green)
    lapElapsedS: number;             // seconds since the current lap started
    lapNumber: number;               // 1-based lap in progress
    lastLapNumber: number | null;    // most recently completed lap (drives the card)
    lastLapTimeS: number | null;
    lastLapEnergyWh: number | null;
}

export interface DashMessage {
    seq: number;
    can: CanData;
    mqtt: MqttData;
    pacing: PacingData;
    // Website-authored lap-card layout (retained lhre/dash/layout), forwarded by
    // dashd. Validated at render; absent → the built-in lap card is used.
    layout?: unknown;
    // Website-authored park/pit-screen layout (retained lhre/dash/parkLayout).
    // Validated at render; absent → the built-in park screen is used.
    parkLayout?: unknown;
}

// VCU event mode enum (matches firmware VCU_DEFAULT_PARAMS + per-event override
// in VCU/firmware/Core/Inc/params/*.h). Anything outside this range is shown
// as the raw integer so a new mode added on the VCU isn't silently dropped.
export const EVENT_MODE_LABELS: Record<number, string> = {
    0: "—",
    1: "ACCEL",
    2: "SKID",
    3: "AUTOX",
    4: "ENDUR",
};

export function eventModeLabel(mode: number | null | undefined): string | null {
    if (mode === null || mode === undefined) return null;
    return EVENT_MODE_LABELS[mode] ?? String(mode);
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
