// TypeScript interfaces matching the WebSocket JSON spec (v1.0)
// See: Dashboard WebSocket JSON Requirements document

export interface CanData {
    // Dynamics
    speed: number | null;           // Vehicle speed [TBD: MPH or m/s]
    power: number | null;           // Motor power in kW (-80 to +80, negative = regen)
    odometer: number | null;        // Total distance [TBD: miles or km]

    // Battery / Pack
    soc: number | null;             // State of charge (0-100%)
    temperature: number | null;     // Battery temp [TBD: °C or °F]

    // Telemetry
    signalStrength: number | null;  // 5G signal (0-4 bars)

    // Shutdown circuit: 16 booleans (true = OK, false = FAULT)
    shutdown: boolean[] | null;
}

export interface MqttData {
    lapDelta: number | null;        // Time delta vs reference lap in seconds
    energyDelta: number | null;     // Energy delta vs target in Wh
    lapsRemaining: number | null;   // Estimated laps remaining
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
