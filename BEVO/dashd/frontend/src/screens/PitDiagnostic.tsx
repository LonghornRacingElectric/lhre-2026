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

    // ----- Unwired fields (frontend stubs only — see top-of-file note) -----

    // BACKEND TODO: CanData.apps         (VCU apps_percent)
    const apps: number | null = null;
    // BACKEND TODO: CanData.bpps         (VCU bpps_percent)
    const bpps: number | null = null;
    // BACKEND TODO: CanData.brakePressureFront / .brakePressureRear
    const brakePressureFront: number | null = null;
    const brakePressureRear: number | null = null;

    // BACKEND TODO: CanData.motorTemp     (Inverter motor temp message)
    const motorTemp: number | null = null;
    // BACKEND TODO: CanData.inverterTemp  (Inverter hotspot or device temp)
    const inverterTemp: number | null = null;
    // BACKEND TODO: CanData.coolantTemp   (HVC or VCU coolant loop temp)
    const coolantTemp: number | null = null;
    // BACKEND TODO: CanData.cellTempMax   (currently only cell_top_temp is forwarded)
    const cellTempMax: number | null = cellTopTemp;
    // BACKEND TODO: CanData.cellTempMin
    const cellTempMin: number | null = null;

    // BACKEND TODO: CanData.hvVoltage     (dashd has pack.dc_bus_v but doesn't pass through)
    const hvVoltage: number | null = null;
    // BACKEND TODO: CanData.hvCurrent     (dashd has pack.dc_bus_current but doesn't pass through)
    const hvCurrent: number | null = null;
    // BACKEND TODO: CanData.lvVoltage
    const lvVoltage: number | null = null;

    // BACKEND TODO: CanData.wheelSpeeds[4] — only a single CanData.speed exists today
    const wheelSpeedFL: number | null = null;
    const wheelSpeedFR: number | null = null;
    const wheelSpeedRL: number | null = null;
    const wheelSpeedRR: number | null = null;

    // ----- Derived: faults from shutdown[] (will stay null until shutdown wires up) -----
    // NOTE: shutdown[] itself is also blocked — the CAN bus exposes 4 legs but the
    // frontend expects 16 named items, and the leg→item mapping is owned by the
    // electrical team and not yet provided. Until then, shutdown is always null.
    const faultIndices = shutdown
        ? shutdown.map((ok, i) => (ok ? -1 : i)).filter(i => i >= 0)
        : null;
    const faultCount = faultIndices ? faultIndices.length : null;
    const firstFaultName =
        faultIndices && faultIndices.length > 0 ? SHUTDOWN_NAMES[faultIndices[0]] : null;

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
                        <>
                            <span className="fault-summary-status bad">
                                {faultCount} ACTIVE
                            </span>
                            {firstFaultName && (
                                <span className="fault-summary-name">{firstFaultName}</span>
                            )}
                        </>
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

            {/* ===== BOTTOM TRAY: HV V | HV A | LV V ===== */}
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
                            {fmt(lvVoltage, 2)}
                            <span className="unit-label">V</span>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
};

export default PitDiagnostic;
