import React from 'react';
import { useDash } from '../context/DashContext';
import { SHUTDOWN_NAMES } from '../types/DashData';
import ConnectivityIndicator from '../components/ConnectivityIndicator';
import './ScreenOne.css';
import './PitDiagnostic.css';

// Pit / Diagnostic Screen
// Static-state info to inspect before/after driving — driver inputs, powertrain
// temps, battery V/I, wheel speeds, active faults.
// Resolution: 800 x 480
//
// =====================================================================
// BACKEND STATUS — most fields on this screen are NOT WIRED YET.
// =====================================================================
// Today, dashd (BEVO/dashd/main.rs) only forwards: speed (single wheel),
// power (derived = dc_bus_v * dc_bus_current / 1000), soc, temperature
// (cell_top_temp), signalStrength (always null), shutdown (always null),
// and lap/energy/laps from MQTT.
//
// To make this screen useful, both ends need extending in lockstep:
//   1. Add fields to CanData in BEVO/dashd/main.rs and populate them in
//      extract_can_data() from the matching protobuf messages.
//   2. Mirror those additions in src/types/DashData.ts so the frontend
//      can read them.
//
// Per-field TODOs are flagged at the data-binding site below.
// =====================================================================

const fmt = (val: number | null | undefined, decimals = 0): string => {
    if (val === null || val === undefined) return '--';
    return decimals > 0 ? val.toFixed(decimals) : Math.round(val).toString();
};

const PitDiagnostic: React.FC = () => {
    const { data } = useDash();

    // ----- Wired fields -----
    const cellTopTemp = data?.can.temperature ?? null;
    const shutdown = data?.can.shutdown ?? null;
    // BACKEND: odometer field exists in CanData but dashd always sets it
    // to null today (no integrated wheel-speed-over-time logic, and unit
    // unverified). Will read whatever the backend eventually provides.
    const odometer = data?.can.odometer ?? null;

    // ----- Optional fields — wired in demo mode via useDemoData; live mode
    //       returns null until dashd publishes them. See per-field BACKEND
    //       TODOs in the type definition (src/types/DashData.ts). -----

    const apps = data?.can.apps ?? null;
    const bpps = data?.can.bpps ?? null;
    const brakePressureFront = data?.can.brakePressureFront ?? null;
    const brakePressureRear = data?.can.brakePressureRear ?? null;

    const motorTemp = data?.can.motorTemp ?? null;
    const inverterTemp = data?.can.inverterTemp ?? null;
    const coolantTemp = data?.can.coolantTemp ?? null;
    // Prefer dashd's pack-wide max (aggregate of pack.cells_temps[]) when
    // present; fall back to the single cell_top_temp the main TEMP gauge uses.
    const cellTempMax = data?.can.cellTempMax ?? cellTopTemp;
    const cellTempAvg = data?.can.cellTempAvg ?? null;
    const cellTempMin = data?.can.cellTempMin ?? null;

    const hvVoltage = data?.can.hvVoltage ?? null;
    const hvCurrent = data?.can.hvCurrent ?? null;
    const lvVoltage = data?.can.lvVoltage ?? null;
    const lvCurrent = data?.can.lvCurrent ?? null;

    const wheelSpeedFL = data?.can.wheelSpeedFL ?? null;
    const wheelSpeedFR = data?.can.wheelSpeedFR ?? null;
    const wheelSpeedRL = data?.can.wheelSpeedRL ?? null;
    const wheelSpeedRR = data?.can.wheelSpeedRR ?? null;

    // Derived faults — dashd emits four legs from diagnostics_low.shutdown_legX.
    // SHUTDOWN_NAMES holds the placeholder leg labels until electrical clarifies
    // which physical sub-system each leg represents.
    const faultIndices = shutdown
        ? shutdown.map((ok, i) => (ok ? -1 : i)).filter(i => i >= 0)
        : null;
    const faultCount = faultIndices ? faultIndices.length : null;

    // ----- Render helpers -----

    const tempClass = (t: number | null): string => {
        if (t === null) return '';
        if (t > 80) return 'bad';
        if (t > 60) return 'warn';
        return '';
    };

    const renderRow = (
        label: string,
        value: number | null,
        unit: string,
        decimals = 0,
        cls: string = ''
    ) => (
        <div className="diag-row">
            <span className="label-small">{label}</span>
            <span className={`value-display diag-value ${cls}`}>
                {fmt(value, decimals)}
                <span className="unit-label">{unit}</span>
            </span>
        </div>
    );

    return (
        <div className="modern-dash-container pit-container">
            {/* ===== TOP TRAY: connectivity + fault summary ===== */}
            <div className="dash-card pit-tray pit-tray-top">
                {/* Left: shared connectivity indicator (mirrors ScreenOne) */}
                <div className="pit-tray-left">
                    <ConnectivityIndicator />
                </div>

                <div className="fault-summary">
                    {faultCount === null ? (
                        <span className="fault-summary-status unknown">--</span>
                    ) : faultCount === 0 ? (
                        <span className="fault-summary-status ok">SYSTEM OK</span>
                    ) : (
                        <span className="fault-summary-status bad">
                            {faultCount} ACTIVE
                        </span>
                    )}
                </div>

                <div className="pit-tray-right">DIAG</div>
            </div>

            {/* ===== MIDDLE: 4 section cards ===== */}
            <div className="pit-middle">
                {/* Driver inputs */}
                <div className="dash-card pit-section">
                    <div className="pit-section-title">DRIVER INPUTS</div>
                    {renderRow('APPS', apps, '%', 1)}
                    {renderRow('BPPS', bpps, '%', 1)}
                    {renderRow('Brk F', brakePressureFront, 'psi', 0)}
                    {renderRow('Brk R', brakePressureRear, 'psi', 0)}
                </div>

                {/* Powertrain temps */}
                <div className="dash-card pit-section">
                    <div className="pit-section-title">POWERTRAIN</div>
                    {renderRow('Motor', motorTemp, '°C', 0, tempClass(motorTemp))}
                    {renderRow('Inv', inverterTemp, '°C', 0, tempClass(inverterTemp))}
                    {renderRow('Cool', coolantTemp, '°C', 0, tempClass(coolantTemp))}
                    {renderRow('Cell↑', cellTempMax, '°C', 0, tempClass(cellTempMax))}
                    {renderRow('Cell μ', cellTempAvg, '°C', 0, tempClass(cellTempAvg))}
                    {renderRow('Cell↓', cellTempMin, '°C', 0)}
                </div>

                {/* Wheels */}
                <div className="dash-card pit-section">
                    <div className="pit-section-title">WHEELS</div>
                    {renderRow('FL', wheelSpeedFL, '', 0)}
                    {renderRow('FR', wheelSpeedFR, '', 0)}
                    {renderRow('RL', wheelSpeedRL, '', 0)}
                    {renderRow('RR', wheelSpeedRR, '', 0)}
                </div>

                {/* Active faults */}
                <div className="dash-card pit-section">
                    <div className="pit-section-title">ACTIVE FAULTS</div>
                    {faultIndices === null ? (
                        <div className="diag-fault-row empty">--</div>
                    ) : faultIndices.length === 0 ? (
                        <div className="diag-fault-row empty">NONE</div>
                    ) : (
                        faultIndices
                            .slice(0, 6)
                            .map(i => (
                                <div key={i} className="diag-fault-row">
                                    {SHUTDOWN_NAMES[i]}
                                </div>
                            ))
                    )}
                    {faultIndices && faultIndices.length > 6 && (
                        <div className="diag-fault-row empty">
                            +{faultIndices.length - 6} more
                        </div>
                    )}
                </div>
            </div>

            {/* ===== BOTTOM TRAY: HV V | HV A | LV V | Odometer ===== */}
            <div className="dash-card pit-tray pit-tray-bottom">
                <div className="pit-batt-grid">
                    <div className="pit-batt-cell">
                        <div className="label-small">HV V</div>
                        <div className="value-display pit-batt-val">
                            {fmt(hvVoltage, 1)}
                            <span className="unit-label">V</span>
                        </div>
                    </div>
                    <div className="pit-batt-cell">
                        <div className="label-small">HV A</div>
                        <div className="value-display pit-batt-val">
                            {fmt(hvCurrent, 1)}
                            <span className="unit-label">A</span>
                        </div>
                    </div>
                    <div className="pit-batt-cell">
                        <div className="label-small">LV V</div>
                        <div className="value-display pit-batt-val">
                            {fmt(lvVoltage, 1)}
                            <span className="unit-label">V</span>
                        </div>
                    </div>
                    <div className="pit-batt-cell">
                        <div className="label-small">LV A</div>
                        <div className="value-display pit-batt-val">
                            {fmt(lvCurrent, 1)}
                            <span className="unit-label">A</span>
                        </div>
                    </div>
                    <div className="pit-batt-cell">
                        <div className="label-small">Odo</div>
                        <div className="value-display pit-batt-val">
                            {fmt(odometer, 1)}
                            <span className="unit-label">mi</span>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
};

export default PitDiagnostic;
