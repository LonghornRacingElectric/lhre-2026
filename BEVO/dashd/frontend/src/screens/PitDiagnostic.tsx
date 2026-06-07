import React from 'react';
import { useDash } from '../context/DashContext';
import { SHUTDOWN_NAMES } from '../types/DashData';
import ConnectivityIndicator from '../components/ConnectivityIndicator';
import { LapCardRenderer } from '../LapCardRenderer';
import { validateLapCardLayout } from '../dashLayout';
import './ScreenOne.css';
import './PitDiagnostic.css';

// Pit / Diagnostic Screen — the PARK debug view.
// Shown automatically whenever the VCU reports PRNDL = Park (see Dashboard.tsx),
// and replaced by the driving screen the instant the car shifts to Drive. It
// surfaces the things the crew checks between runs at an FSAE event: state of
// energy, cell-voltage health (min/max/spread), cell + powertrain temps, driver
// inputs + brake bias, running VCU energy usage, pack/LV rails, and active faults.
// Resolution: 800 x 480.
//
// Two render paths:
//   1. If trackside has published a custom park layout (retained
//      lhre/dash/parkLayout, forwarded as data.parkLayout), render it with the
//      shared LapCardRenderer — same authoring loop as the lap card.
//   2. Otherwise the built-in grid below (so the screen never blanks).
// All fields read from the OrionSensorData snapshot dashd forwards over the
// WebSocket (BEVO/dashd/main.rs::extract_can_data). A field shows "--" until
// cand has decoded the matching CAN packet.

const fmt = (val: number | null | undefined, decimals = 0): string => {
    if (val === null || val === undefined) return '--';
    return decimals > 0 ? val.toFixed(decimals) : Math.round(val).toString();
};

// Energy: Wh below 1 kWh, kWh above, so a multi-kWh session total stays readable.
const fmtEnergy = (wh: number | null | undefined): string => {
    if (wh === null || wh === undefined) return '--';
    return Math.abs(wh) >= 1000 ? `${(wh / 1000).toFixed(2)}` : `${Math.round(wh)}`;
};
const energyUnit = (wh: number | null | undefined): string =>
    wh !== null && wh !== undefined && Math.abs(wh) >= 1000 ? 'kWh' : 'Wh';

const PitDiagnostic: React.FC = () => {
    const { data } = useDash();
    // Follow the dash's current light/dark theme (body.theme-light is toggled by
    // the car's settings / auto sunrise-sunset), same mechanism ScreenOne uses.
    const cardTheme: 'dark' | 'light' =
        (typeof document !== 'undefined' && document.body.classList.contains('theme-light')) ? 'light' : 'dark';
    const can = data?.can;

    // Custom park layout authored on the website (validated; falls back to the
    // built-in grid below when absent/malformed so the screen never blanks).
    const customLayout = validateLapCardLayout(data?.parkLayout);
    if (customLayout && customLayout.widgets.length) {
        const ctx = { can: data?.can, pacing: data?.pacing, mqtt: data?.mqtt };
        return (
            <div style={{ position: 'absolute', inset: 0, zIndex: 1 }}>
                <LapCardRenderer layout={customLayout} data={ctx} scale={1} theme={cardTheme} />
            </div>
        );
    }

    // ----- Energy / charge -----
    const soe = can?.soc ?? null;                       // state of energy %
    const vcuNet = can?.vcuNetEnergyWh ?? null;         // running net (drive - regen) Wh
    const vcuRegen = can?.vcuRegenEnergyWh ?? null;     // running regen Wh
    // Drive (gross out of the pack) = net + regen returned.
    const vcuDrive = vcuNet !== null && vcuRegen !== null ? vcuNet + vcuRegen : null;

    // ----- Driver inputs -----
    const apps = can?.apps ?? null;
    const bpps = can?.bpps ?? null;
    const brakeBias = can?.brakeBias ?? null;
    const brakePressureFront = can?.brakePressureFront ?? null;
    const brakePressureRear = can?.brakePressureRear ?? null;

    // ----- Cell voltages (pack health) -----
    const cellVMax = can?.cellVMax ?? null;
    const cellVMin = can?.cellVMin ?? null;
    const cellVSpread = can?.cellVSpread ?? null;       // V; shown as mV
    const cellVSpreadMv = cellVSpread !== null ? cellVSpread * 1000 : null;

    // ----- Cell + powertrain temps -----
    const cellTempMax = can?.cellTempMax ?? can?.temperature ?? null;
    const cellTempAvg = can?.cellTempAvg ?? null;
    const cellTempMin = can?.cellTempMin ?? null;
    const motorTemp = can?.motorTemp ?? null;
    const inverterTemp = can?.inverterTemp ?? null;
    const coolantTemp = can?.coolantTemp ?? null;

    // ----- Rails -----
    const hvVoltage = can?.hvVoltage ?? null;
    const hvCurrent = can?.hvCurrent ?? null;
    const lvVoltage = can?.lvVoltage ?? null;
    const lvCurrent = can?.lvCurrent ?? null;
    const power = can?.power ?? null;

    // ----- Faults -----
    const shutdown = can?.shutdown ?? null;
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
    // Cell imbalance: >100 mV is a real concern, >50 mV worth watching.
    const spreadClass = (mv: number | null): string => {
        if (mv === null) return '';
        if (mv > 100) return 'bad';
        if (mv > 50) return 'warn';
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
            {/* ===== TOP TRAY: connectivity + fault summary + SoE hero ===== */}
            <div className="dash-card pit-tray pit-tray-top">
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

                {/* SoE is the headline pit number — big, top-right. */}
                <div className="pit-soe">
                    <span className="label-small">SoE</span>
                    <span className="pit-soe-val">
                        {fmt(soe, 0)}<span className="unit-label">%</span>
                    </span>
                </div>
            </div>

            {/* ===== MIDDLE: 3 x 2 section cards ===== */}
            <div className="pit-middle">
                {/* Driver inputs + brake bias */}
                <div className="dash-card pit-section">
                    <div className="pit-section-title">DRIVER INPUTS</div>
                    {renderRow('APPS', apps, '%', 1)}
                    {renderRow('BPPS', bpps, '%', 1)}
                    {renderRow('Bias F', brakeBias, '%', 0)}
                    {/* Front / rear brake pressure on one row to keep the card to 4 rows. */}
                    <div className="diag-row">
                        <span className="label-small">Brk F/R</span>
                        <span className="value-display diag-value">
                            {fmt(brakePressureFront, 0)} / {fmt(brakePressureRear, 0)}
                            <span className="unit-label">psi</span>
                        </span>
                    </div>
                </div>

                {/* Cell voltage health */}
                <div className="dash-card pit-section">
                    <div className="pit-section-title">CELL VOLTAGE</div>
                    {renderRow('Max', cellVMax, 'V', 3)}
                    {renderRow('Min', cellVMin, 'V', 3)}
                    {renderRow('Δ Spread', cellVSpreadMv, 'mV', 0, spreadClass(cellVSpreadMv))}
                </div>

                {/* Running VCU energy */}
                <div className="dash-card pit-section">
                    <div className="pit-section-title">ENERGY · VCU</div>
                    <div className="diag-row">
                        <span className="label-small">Net</span>
                        <span className="value-display diag-value">
                            {fmtEnergy(vcuNet)}<span className="unit-label">{energyUnit(vcuNet)}</span>
                        </span>
                    </div>
                    <div className="diag-row">
                        <span className="label-small">Drive</span>
                        <span className="value-display diag-value">
                            {fmtEnergy(vcuDrive)}<span className="unit-label">{energyUnit(vcuDrive)}</span>
                        </span>
                    </div>
                    <div className="diag-row">
                        <span className="label-small">Regen</span>
                        <span className="value-display diag-value good">
                            {fmtEnergy(vcuRegen)}<span className="unit-label">{energyUnit(vcuRegen)}</span>
                        </span>
                    </div>
                </div>

                {/* Cell temps */}
                <div className="dash-card pit-section">
                    <div className="pit-section-title">CELL TEMP</div>
                    {renderRow('Max', cellTempMax, '°C', 0, tempClass(cellTempMax))}
                    {renderRow('Avg', cellTempAvg, '°C', 0, tempClass(cellTempAvg))}
                    {renderRow('Min', cellTempMin, '°C', 0)}
                </div>

                {/* Powertrain temps */}
                <div className="dash-card pit-section">
                    <div className="pit-section-title">POWERTRAIN</div>
                    {renderRow('Motor', motorTemp, '°C', 0, tempClass(motorTemp))}
                    {renderRow('Inv', inverterTemp, '°C', 0, tempClass(inverterTemp))}
                    {renderRow('Cool', coolantTemp, '°C', 0, tempClass(coolantTemp))}
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
                            .slice(0, 4)
                            .map(i => (
                                <div key={i} className="diag-fault-row">
                                    {SHUTDOWN_NAMES[i]}
                                </div>
                            ))
                    )}
                    {faultIndices && faultIndices.length > 4 && (
                        <div className="diag-fault-row empty">
                            +{faultIndices.length - 4} more
                        </div>
                    )}
                </div>
            </div>

            {/* ===== BOTTOM TRAY: HV V | HV A | LV V | LV A | Pack kW ===== */}
            <div className="dash-card pit-tray pit-tray-bottom">
                <div className="pit-batt-grid">
                    <div className="pit-batt-cell">
                        <div className="label-small">HV V</div>
                        <div className="value-display pit-batt-val">
                            {fmt(hvVoltage, 1)}<span className="unit-label">V</span>
                        </div>
                    </div>
                    <div className="pit-batt-cell">
                        <div className="label-small">HV A</div>
                        <div className="value-display pit-batt-val">
                            {fmt(hvCurrent, 1)}<span className="unit-label">A</span>
                        </div>
                    </div>
                    <div className="pit-batt-cell">
                        <div className="label-small">LV V</div>
                        <div className="value-display pit-batt-val">
                            {fmt(lvVoltage, 1)}<span className="unit-label">V</span>
                        </div>
                    </div>
                    <div className="pit-batt-cell">
                        <div className="label-small">LV A</div>
                        <div className="value-display pit-batt-val">
                            {fmt(lvCurrent, 1)}<span className="unit-label">A</span>
                        </div>
                    </div>
                    <div className="pit-batt-cell">
                        <div className="label-small">Pack</div>
                        <div className="value-display pit-batt-val">
                            {fmt(power, 1)}<span className="unit-label">kW</span>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
};

export default PitDiagnostic;
