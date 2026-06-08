import React from 'react';
import ConnectivityIndicator from '../components/ConnectivityIndicator';
import { useDash } from '../context/DashContext';
import { useEnergyPacing } from '../hooks/useEnergyPacing';
import { LapCardRenderer } from '../LapCardRenderer';
import { validateLapCardLayout } from '../dashLayout';
import { eventModeLabel } from '../types/DashData';
import './ScreenOne.css';

// Screen One: Main Dashboard (Modern EV Style)
// Resolution: 800 x 480

const ScreenOne: React.FC = () => {
    const { data } = useDash();

    // Endurance energy pacing: integrates CAN power on-board against the
    // trackside-set targetPower budget, resets each lap on lapTrigger, and
    // surfaces a full-screen lap card when a lap closes. See useEnergyPacing.
    const pacing = useEnergyPacing(data);
    // Budget held last-known across a dropout — dim it + badge it so the driver
    // knows it's no longer live rather than trusting a frozen number.
    const targetPowerStale = data?.mqtt.targetPowerStale ?? false;
    // ±Wh that pegs the energy-budget bar end-to-end. A lap is a few hundred
    // Wh, so ~150 Wh of margin/overage is a meaningful full-scale deflection.
    const ENERGY_DELTA_MAX_WH = 150;

    // Extract values with null fallback
    const speed = data?.can.speed;
    const power = data?.can.power;
    // Lap card + message overlays follow the car's current light/dark theme
    // (body.theme-light is toggled by SettingsContext / auto sunrise-sunset).
    const dashTheme: 'light' | 'dark' =
        (typeof document !== 'undefined' && document.body.classList.contains('theme-light')) ? 'light' : 'dark';
    const charge = data?.can.soc;
    const temp = data?.can.temperature;
    const lapDelta = data?.mqtt.lapDelta;
    const energyDelta = data?.mqtt.energyDelta;
    const lapsRemaining = data?.mqtt.lapsRemaining;

    // ----- Driver-thread proposals -----
    // These are declared optional in DashData.ts. Demo mode synthesises
    // values via useDemoData; live mode (dashd) does not yet emit them, so
    // they fall back to null in production until the backend is extended.
    // BACKEND TODO: dashd needs to publish MqttData.lapsRemainingEnergy
    //   (off-car prediction from SOC + average consumption — see
    //   BEVO/dashd/MQTT_CONTRACT.md).
    const lapsRemainingEnergy = data?.mqtt.lapsRemainingEnergy ?? null;
    // BACKEND TODO: dashd needs MqttData.bestLapTime / .lastLapTime
    //   (off-car) and on-car timer for currentLapTime (driven by lap signal).
    const bestLapTime = data?.mqtt.bestLapTime ?? null;
    const lastLapTime = data?.mqtt.lastLapTime ?? null;
    const currentLapTime = data?.mqtt.currentLapTime ?? null;
    // BACKEND TODO: dashd needs CanData.brakeBias derived from front/rear
    //   brake pressures. Bouncy on Angelique — needs low-pass filter and
    //   should probably gate display by brake pressure > threshold.
    const brakeBias = data?.can.brakeBias ?? null;
    // BACKEND TODO: real source UNKNOWN. Likely CAN from VCU
    //   (steering-wheel rotary + enable switch), but could be MQTT-side
    //   settings. UI is wired only against demo hook; live mode will
    //   show "OFF/—" until backend producer is defined.
    const tcLevel = data?.can.tcLevel ?? null;
    const tcEnabled = data?.can.tcEnabled ?? null;
    const regenEnabled = data?.can.regenEnabled ?? null;
    // Active VCU event mode (Controls.event_mode). 0/null shows "—"; 1-4 map
    // to ACCEL/SKID/AUTOX/ENDUR via EVENT_MODE_LABELS in DashData.ts.
    const eventMode = data?.can.eventMode ?? null;
    const eventModeText = eventModeLabel(eventMode);
    const eventModeActive = eventMode !== null && eventMode !== undefined && eventMode !== 0;
    // BACKEND TODO: dashd needs MqttData.lapDeltaRate — d(lapDelta)/dt,
    //   units of seconds per second. Drivers want this as the primary
    //   glance bar because absolute delta lags. Compute off-car to keep
    //   the dash simple, or sample lapDelta on-car and low-pass-filter
    //   the diff.
    const lapDeltaRate = data?.mqtt.lapDeltaRate ?? null;
    const LAP_DELTA_RATE_MAX = 0.5; // ±0.5 s/s pegs the bar end-to-end

    // VCU PRNDL state — "P" or "D". Null = no diagnostics_high data yet.
    const prndl = data?.can.prndl ?? null;

    // HV status from HVC 0x131. posContactor=1 means energized (ready
    // to drive). We derive "precharging / wait" from negContactor=1 &&
    // posContactor=0 (shutdown closed, HV side not yet engaged) instead
    // of the prechargeContactor CAN field — the latter is transient and
    // not reliably observable, but the neg-closed-pos-open window is a
    // clean indicator of the precharge sequence. When neg=0 the dash
    // should switch to the shutdown screen entirely (TODO not yet done).
    const posContactor = data?.can.posContactor ?? null;
    const negContactor = data?.can.negContactor ?? null;

    // Derived: alerts from temperature thresholds (60 °C cell limit).
    // The shutdown-circuit alert is intentionally absent: shutdown_leg
    // signals are firing false FAULTs (driveday 2026-05-24). Pit/diag
    // screens still surface real per-leg state via data.can.shutdown.
    const alerts: string[] = [];
    if (temp !== null && temp !== undefined && temp > 55) {
        alerts.push("High Battery Temp");
    }

    // -------------------------------------------------------------------------
    // RENDER HELPERS
    // -------------------------------------------------------------------------

    const getDeltaColor = (val: number | null | undefined, inverse: boolean = false) => {
        if (val === null || val === undefined || val === 0) return "var(--fg-muted)";
        if (val < 0) return inverse ? "#FF3333" : "#00FF66";
        return inverse ? "#00FF66" : "#FF3333";
    };

    // Format a nullable number, showing "--" when null
    const fmt = (val: number | null | undefined, decimals: number = 0): string => {
        if (val === null || val === undefined) return "--";
        return decimals > 0 ? val.toFixed(decimals) : Math.round(val).toString();
    };

    // Format seconds as M:SS.ss (e.g. 83.45 -> "1:23.45"). Returns "--:--.--"
    // when null so the slot is visibly waiting for data.
    const fmtLapTime = (secs: number | null | undefined): string => {
        if (secs === null || secs === undefined) return "--:--.--";
        const m = Math.floor(secs / 60);
        const s = secs - m * 60;
        return `${m}:${s.toFixed(2).padStart(5, '0')}`;
    };

    const BRAND_COLOR = "#BF5700"; // Burnt Orange

    // Safe numeric values for gauges (default to 0 when null)
    const safePower = power ?? 0;
    const safeCharge = charge ?? 0;
    const safeTemp = temp ?? 0;

    // Power bar split: regen takes the leftmost 20%, drive takes the
    // remaining 80%. Max-out values for each side are scaled so the bar
    // fill is linear within each half.
    const REGEN_PCT = 20;
    const DRIVE_PCT = 100 - REGEN_PCT;
    const REGEN_MAX_KW = 20;
    const DRIVE_MAX_KW = 80;

    return (
        <div className="modern-dash-container" style={{ width: '100vw', height: '100vh', position: 'relative' }}>

            {/* Alerts Overlay */}
            {alerts.length > 0 && (
                <div className="alert-overlay">
                    {alerts.join(" • ")}
                </div>
            )}

            {/* Lap Delta Panel - Top Center */}
            <div className="dash-card" style={{
                position: 'absolute',
                top: '0',
                left: '0',
                zIndex: 100,
                height: '60px',
                width: '100%',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                borderRadius: '0',
                background: 'var(--card-bg)',
                backdropFilter: 'blur(12px)',
                border: '1px solid var(--card-border)',
                borderTop: 'none',
                boxShadow: 'var(--card-shadow)'
            }}>
                {/* Left: SES laps remaining (fixed value width to avoid layout shift) */}
                <div style={{ position: 'absolute', left: '30px', top: '50%', transform: 'translateY(-50%)', display: 'flex', alignItems: 'baseline', gap: '10px' }}>
                    <span className="label-small" style={{ marginBottom: 0 }}>SES</span>
                    <span className="value-display" style={{ fontSize: '2.2rem', fontWeight: 'bold', lineHeight: 1, minWidth: '48px', textAlign: 'left', display: 'inline-block' }}>
                        {fmt(lapsRemaining, 0)}
                    </span>
                </div>

                {/* Center: full-width s/s rate bar (label on left) */}
                <div style={{
                    position: 'absolute',
                    left: '50%',
                    top: '50%',
                    transform: 'translate(-50%, -50%)',
                    zIndex: 100,
                    height: '100%',
                    width: '540px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '10px',
                    padding: '0 8px'
                }}>
                    <span className="label-small" style={{ marginBottom: 0, flexShrink: 0 }}>S/S</span>
                    <div style={{ flex: 1, height: '26px', background: 'var(--bar-track)', borderRadius: '4px', position: 'relative', overflow: 'hidden' }}>
                        {/* Zero marker (dead center) */}
                        <div style={{ position: 'absolute', left: '50%', top: 0, bottom: 0, width: '2px', background: 'var(--bar-center)', transform: 'translateX(-50%)', zIndex: 2 }} />
                        {/* Fill */}
                        <div style={{
                            position: 'absolute',
                            top: 0, bottom: 0,
                            left: lapDeltaRate !== null && lapDeltaRate < 0
                                ? `${50 - (Math.min(Math.abs(lapDeltaRate), LAP_DELTA_RATE_MAX) / LAP_DELTA_RATE_MAX) * 50}%`
                                : '50%',
                            width: lapDeltaRate !== null
                                ? `${(Math.min(Math.abs(lapDeltaRate), LAP_DELTA_RATE_MAX) / LAP_DELTA_RATE_MAX) * 50}%`
                                : '0%',
                            background: lapDeltaRate !== null && lapDeltaRate < 0
                                ? 'linear-gradient(to right, #00CC00, #00FF66)'
                                : 'linear-gradient(to left, #FF3333, #FF6600)',
                            transition: 'all 0.1s linear'
                        }} />
                    </div>
                </div>

                {/* Right: NRG laps remaining (fixed value width) */}
                <div style={{ position: 'absolute', right: '30px', top: '50%', transform: 'translateY(-50%)', display: 'flex', alignItems: 'baseline', gap: '10px' }}>
                    <span className="label-small" style={{ marginBottom: 0 }}>NRG</span>
                    <span className="value-display" style={{ fontSize: '2.2rem', fontWeight: 'bold', lineHeight: 1, minWidth: '48px', textAlign: 'left', display: 'inline-block' }}>
                        {fmt(lapsRemainingEnergy, 0)}
                    </span>
                </div>
            </div>

            {/* Left Side Panel - The whole sidebar IS the temp gauge.
                Fill rises from the bottom in proportion to the value.
                Label sits at the top, digits at the bottom. */}
            <div className="dash-card" style={{
                position: 'absolute',
                top: '60px',
                bottom: '80px',
                left: '0',
                zIndex: 100,
                width: '90px',
                borderRadius: '0',
                background: 'var(--card-bg)',
                backdropFilter: 'blur(12px)',
                border: '1px solid var(--card-border)',
                boxShadow: 'var(--card-shadow)',
                padding: 0,
                overflow: 'hidden',
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'space-between'
            }}>
                {/* Fill bar — inset 4px on three sides, anchored to bottom.
                    Scale: 0–60 °C (cell temperature limit). */}
                <div style={{
                    position: 'absolute',
                    bottom: 4,
                    left: 4,
                    right: 4,
                    height: `calc((100% - 8px) * ${Math.min(Math.max(safeTemp, 0), 60) / 60})`,
                    background: safeTemp > 50 ? '#ff0000' : BRAND_COLOR,
                    transition: 'height 0.3s ease-in-out, background-color 0.3s',
                    zIndex: 0
                }} />

                <div className="label-small text-center" style={{
                    position: 'relative',
                    zIndex: 2,
                    marginTop: '12px',
                    marginBottom: 0,
                    fontSize: '1rem',
                    letterSpacing: '2px',
                    color: 'var(--fg-primary)',
                    textShadow: 'var(--text-shadow)'
                }}>TEMP</div>

                <div className="text-center value-display" style={{
                    position: 'relative',
                    zIndex: 2,
                    marginBottom: '12px',
                    fontSize: '2.4rem',
                    fontWeight: 'bold',
                    lineHeight: 1,
                    color: 'var(--fg-primary)',
                    textShadow: 'var(--text-shadow-strong)'
                }}>
                    {temp !== null && temp !== undefined ? Math.round(temp) : "--"}
                    <span style={{ fontSize: '1rem', marginLeft: '4px', color: 'var(--fg-secondary)' }}>°C</span>
                </div>
            </div>

            {/* Right Side Panel - The whole sidebar IS the SOC gauge.
                Same pattern as the left: fill rises from the bottom. */}
            <div className="dash-card" style={{
                position: 'absolute',
                top: '60px',
                bottom: '80px',
                right: '0',
                zIndex: 100,
                width: '90px',
                borderRadius: '0',
                background: 'var(--card-bg)',
                backdropFilter: 'blur(12px)',
                border: '1px solid var(--card-border)',
                boxShadow: 'var(--card-shadow)',
                padding: 0,
                overflow: 'hidden',
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'space-between'
            }}>
                {/* Fill bar — inset 4px on three sides, anchored to bottom */}
                <div style={{
                    position: 'absolute',
                    bottom: 4,
                    left: 4,
                    right: 4,
                    height: `calc((100% - 8px) * ${Math.min(Math.max(safeCharge, 0), 100) / 100})`,
                    background: '#FFD700',
                    transition: 'height 0.3s ease-in-out',
                    zIndex: 0
                }} />

                <div className="label-small text-center" style={{
                    position: 'relative',
                    zIndex: 2,
                    marginTop: '12px',
                    marginBottom: 0,
                    fontSize: '1rem',
                    letterSpacing: '2px',
                    color: 'var(--fg-primary)',
                    textShadow: 'var(--text-shadow)'
                }}>SOC</div>

                <div className="text-center value-display" style={{
                    position: 'relative',
                    zIndex: 2,
                    marginBottom: '12px',
                    fontSize: '2.4rem',
                    fontWeight: 'bold',
                    lineHeight: 1,
                    color: 'var(--fg-primary)',
                    textShadow: 'var(--text-shadow-strong)'
                }}>
                    {fmt(charge)}
                    <span style={{ fontSize: '1rem', marginLeft: '4px', color: 'var(--fg-secondary)' }}>%</span>
                </div>
            </div>

            {/* Endurance energy-budget bar — sits at the top of the driving
                area, between the temp/SOC sidebars. Center-zero like the power
                and s/s bars: green grows left when banking margin (under the
                trackside power budget), red grows right when over. Resets each
                lap via lapTrigger. The reference rate is the live targetPower
                the strategist dials in on the trackside Dash tab. */}
            <div style={{
                position: 'absolute',
                top: '64px',
                left: '90px',
                right: '90px',
                height: '24px',
                zIndex: 100,
                display: 'flex',
                alignItems: 'center',
                gap: '10px',
                padding: '0 20px'
            }}>
                <span className="label-small" style={{ marginBottom: 0, flexShrink: 0, display: 'flex', flexDirection: 'column', lineHeight: 1.05 }}>
                    <span>NRG</span>
                    {pacing.lapBudgetWh != null ? <span style={{ fontSize: '0.6rem', color: 'var(--fg-secondary)', whiteSpace: 'nowrap' }}>{pacing.lapBudgetWh.toFixed(0)} Wh/lap</span> : null}
                </span>
                <div style={{ flex: 1, height: '22px', background: 'var(--bar-track)', borderRadius: '4px', position: 'relative', overflow: 'hidden', opacity: targetPowerStale ? 0.4 : 1 }}>
                    {/* Zero marker (dead center) */}
                    <div style={{ position: 'absolute', left: '50%', top: 0, bottom: 0, width: '2px', background: 'var(--bar-center)', transform: 'translateX(-50%)', zIndex: 2 }} />
                    {/* Fill — green left = under budget, red right = over budget */}
                    {pacing.budgetDeltaWh !== null && (
                        <div style={{
                            position: 'absolute',
                            top: 0, bottom: 0,
                            left: pacing.budgetDeltaWh < 0
                                ? `${50 - (Math.min(Math.abs(pacing.budgetDeltaWh), ENERGY_DELTA_MAX_WH) / ENERGY_DELTA_MAX_WH) * 50}%`
                                : '50%',
                            width: `${(Math.min(Math.abs(pacing.budgetDeltaWh), ENERGY_DELTA_MAX_WH) / ENERGY_DELTA_MAX_WH) * 50}%`,
                            background: pacing.budgetDeltaWh < 0
                                ? 'linear-gradient(to right, #00CC00, #00FF66)'
                                : 'linear-gradient(to left, #FF3333, #FF6600)',
                            transition: 'all 0.2s linear'
                        }} />
                    )}
                </div>
                {/* Right: target power + signed Wh delta (fixed width, no shift) */}
                <span className="value-display" style={{
                    fontSize: '1.1rem',
                    fontWeight: 'bold',
                    lineHeight: 1,
                    minWidth: '128px',
                    textAlign: 'right',
                    color: targetPowerStale
                        ? 'var(--fg-muted)'
                        : pacing.budgetDeltaWh === null
                            ? 'var(--fg-muted)'
                            : pacing.budgetDeltaWh < 0 ? '#00FF66' : '#FF3333',
                    whiteSpace: 'nowrap'
                }}>
                    {targetPowerStale && (
                        <span className="label-small" style={{ fontSize: '0.6rem', color: '#FF6600', marginRight: '6px', letterSpacing: '1px' }}>STALE</span>
                    )}
                    {pacing.targetPowerKw === null ? '-- kW' : `${pacing.targetPowerKw.toFixed(0)} kW`}
                    {pacing.budgetDeltaWh !== null && (
                        <span style={{ marginLeft: '8px' }}>
                            {pacing.budgetDeltaWh > 0 ? '+' : pacing.budgetDeltaWh < 0 ? '-' : ''}
                            {Math.abs(pacing.budgetDeltaWh).toFixed(0)}
                            <span className="label-small" style={{ fontSize: '0.7rem', marginLeft: '2px' }}>Wh</span>
                        </span>
                    )}
                </span>
            </div>

            {/* Inner square: 2-row grid spanning between sidebars and trays.
                Top row splits into Speed (left) + Lap-stuff table (right).
                Bottom row hosts the big bidirectional power bar + readouts.
                Top pushed to 92px to clear the energy-budget bar above. */}
            <div style={{
                position: 'absolute',
                top: '92px',
                bottom: '80px',
                left: '90px',
                right: '90px',
                display: 'grid',
                gridTemplateColumns: '2fr 3fr',
                gridTemplateRows: '3fr 2fr',
                padding: '14px 20px'
            }}>
                {/* TL: Speed (huge), left-aligned */}
                <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'flex-start', justifyContent: 'center' }}>
                    <div className="value-display" style={{ fontSize: '11rem', fontWeight: 'bold', lineHeight: 0.85 }}>
                        {fmt(speed)}
                    </div>
                    <div className="label-small" style={{ fontSize: '1.2rem', letterSpacing: '4px', marginTop: '4px' }}>MPH</div>
                </div>

                {/* TR: Lap-times table (2x2): CURR | Δ / BEST | LAST */}
                <div style={{
                    display: 'grid',
                    gridTemplateColumns: '1fr 1fr',
                    gridTemplateRows: '1fr 1fr',
                    gap: '8px',
                    padding: '4px'
                }}>
                    {([
                        { label: 'CURR', value: fmtLapTime(currentLapTime), labelColor: '#FFD700', valueColor: '#FFD700' },
                        {
                            label: 'LAP DELTA',
                            value: lapDelta !== null && lapDelta !== undefined
                                ? `${lapDelta > 0 ? '+' : lapDelta < 0 ? '-' : ''}${Math.abs(lapDelta).toFixed(2)} s`
                                : '--',
                            valueColor: getDeltaColor(lapDelta),
                        },
                        { label: 'BEST', value: fmtLapTime(bestLapTime) },
                        { label: 'LAST', value: fmtLapTime(lastLapTime) },
                    ] as { label: string; value: string; labelColor?: string; valueColor?: string }[]).map((cell) => (
                        <div key={cell.label} style={{
                            display: 'flex',
                            flexDirection: 'column',
                            justifyContent: 'center',
                            alignItems: 'flex-start',
                            paddingLeft: '12px',
                            borderLeft: '2px solid var(--divider)'
                        }}>
                            <div className="label-small" style={{
                                fontSize: '0.95rem',
                                letterSpacing: '2px',
                                marginBottom: '4px',
                                color: cell.labelColor ?? 'var(--fg-muted)'
                            }}>{cell.label}</div>
                            <div className="value-display" style={{
                                fontSize: '2.1rem',
                                fontWeight: 'bold',
                                lineHeight: 1,
                                color: cell.valueColor ?? 'var(--fg-primary)'
                            }}>
                                {cell.value}
                            </div>
                        </div>
                    ))}
                </div>

                {/* Bottom row: kW + NRG row on top, big power bar underneath */}
                <div style={{
                    gridColumn: '1 / -1',
                    display: 'flex',
                    flexDirection: 'column',
                    alignItems: 'center',
                    justifyContent: 'center',
                    gap: '10px'
                }}>
                    {/* Side-by-side: USED / BUDGET (energy) + PWR + BB, big */}
                    <div style={{ display: 'flex', justifyContent: 'space-around', alignItems: 'flex-end', width: '95%', gap: '16px' }}>
                        <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center' }}>
                            <div className="label-small" style={{ fontSize: '0.95rem', letterSpacing: '3px', color: 'var(--fg-muted)', marginBottom: '2px' }}>USED</div>
                            <div className="value-display" style={{ fontSize: '2.6rem', fontWeight: 'bold', lineHeight: 1 }}>
                                {pacing.lapEnergyWh != null ? pacing.lapEnergyWh.toFixed(0) : '--'}
                                <span className="label-small" style={{ fontSize: '0.9rem', marginLeft: '3px' }}>Wh</span>
                            </div>
                        </div>
                        <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center' }}>
                            <div className="label-small" style={{ fontSize: '0.95rem', letterSpacing: '3px', color: 'var(--fg-muted)', marginBottom: '2px' }}>BUDGET</div>
                            <div className="value-display" style={{ fontSize: '2.6rem', fontWeight: 'bold', lineHeight: 1, color: pacing.budgetDeltaWh == null ? 'var(--fg-primary)' : pacing.budgetDeltaWh < 0 ? '#00FF66' : '#FF3333' }}>
                                {pacing.lapBudgetWh != null ? pacing.lapBudgetWh.toFixed(0) : '--'}
                                <span className="label-small" style={{ fontSize: '0.9rem', marginLeft: '3px' }}>Wh/lap</span>
                            </div>
                        </div>
                        <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center' }}>
                            <div className="label-small" style={{ fontSize: '0.95rem', letterSpacing: '3px', color: 'var(--fg-muted)', marginBottom: '2px' }}>PWR</div>
                            <div className="value-display" style={{ fontSize: '2.6rem', fontWeight: 'bold', lineHeight: 1 }}>
                                {fmt(power)}
                                <span className="label-small" style={{ fontSize: '0.9rem', marginLeft: '3px' }}>kW</span>
                            </div>
                        </div>
                        <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center' }}>
                            <div className="label-small" style={{ fontSize: '0.95rem', letterSpacing: '3px', color: 'var(--fg-muted)', marginBottom: '2px' }}>BB</div>
                            <div className="value-display" style={{ fontSize: '2.6rem', fontWeight: 'bold', lineHeight: 1, color: 'var(--fg-secondary)' }}>
                                {brakeBias !== null && brakeBias !== undefined ? brakeBias.toFixed(0) : '--'}
                                <span className="label-small" style={{ fontSize: '0.9rem', marginLeft: '3px' }}>%</span>
                            </div>
                        </div>
                    </div>

                    {/* Big power bar — 20% regen on the left, 80% drive on the right */}
                    <div style={{ width: '95%', height: '22px', background: 'var(--bar-track)', borderRadius: '4px', position: 'relative', overflow: 'hidden' }}>
                        {/* Zero marker (where regen meets drive) */}
                        <div style={{ position: 'absolute', left: `${REGEN_PCT}%`, top: 0, bottom: 0, width: '2px', background: 'var(--bar-center)', transform: 'translateX(-50%)', zIndex: 2 }} />
                        {/* Fill */}
                        <div style={{
                            position: 'absolute',
                            top: 0, bottom: 0,
                            left: safePower < 0
                                ? `${REGEN_PCT - (Math.min(Math.abs(safePower), REGEN_MAX_KW) / REGEN_MAX_KW) * REGEN_PCT}%`
                                : `${REGEN_PCT}%`,
                            width: safePower < 0
                                ? `${(Math.min(Math.abs(safePower), REGEN_MAX_KW) / REGEN_MAX_KW) * REGEN_PCT}%`
                                : `${(Math.min(safePower, DRIVE_MAX_KW) / DRIVE_MAX_KW) * DRIVE_PCT}%`,
                            background: safePower < 0 ? 'linear-gradient(to right, #00CC00, #00FF66)' : 'linear-gradient(to left, #FF0000, #BF5700)',
                            transition: 'all 0.1s linear'
                        }} />
                    </div>
                </div>
            </div>

            {/* Bottom Panel - Energy Delta & Odometer */}
            <div className="dash-card" style={{
                position: 'absolute',
                bottom: '0',
                left: '0',
                zIndex: 100,
                height: '80px',
                width: '100%',
                borderRadius: '0',
                background: 'var(--card-bg)',
                backdropFilter: 'blur(12px)',
                border: '1px solid var(--card-border)',
                boxShadow: 'var(--card-shadow)',
                padding: 0
            }}>
                {/* Driver-armable flags (TC + REGEN) — left side, vertically
                    centered. NOT HOOKED UP: values come from the demo hook
                    only; real backend source not yet defined. */}
                <div style={{
                    position: 'absolute',
                    left: '14px',
                    top: '50%',
                    transform: 'translateY(-50%)',
                    width: '160px',
                    display: 'flex',
                    flexDirection: 'column',
                    gap: '4px'
                }}>
                    {/* TC bar */}
                    <div style={{
                        height: '26px',
                        padding: '0 14px',
                        borderRadius: '5px',
                        border: `2px solid ${tcEnabled ? '#BF5700' : 'var(--card-border-hover)'}`,
                        background: tcEnabled ? 'rgba(191,87,0,0.20)' : 'var(--card-bg)',
                        display: 'flex',
                        justifyContent: 'space-between',
                        alignItems: 'center',
                        gap: '8px'
                    }}>
                        <span className="label-small" style={{ marginBottom: 0, fontSize: '0.9rem', letterSpacing: '2px' }}>TC</span>
                        <span className="value-display" style={{
                            fontSize: '1.3rem',
                            fontWeight: 'bold',
                            lineHeight: 1,
                            color: tcEnabled ? 'var(--fg-primary)' : 'var(--fg-muted)'
                        }}>
                            {tcEnabled === null || tcEnabled === undefined
                                ? '—'
                                : tcEnabled && tcLevel !== null && tcLevel !== undefined
                                    ? `L${tcLevel}`
                                    : 'OFF'}
                        </span>
                    </div>

                    {/* REGEN bar */}
                    <div style={{
                        height: '26px',
                        padding: '0 14px',
                        borderRadius: '5px',
                        border: `2px solid ${regenEnabled ? '#00CC66' : 'var(--card-border-hover)'}`,
                        background: regenEnabled ? 'rgba(0,204,102,0.20)' : 'var(--card-bg)',
                        display: 'flex',
                        justifyContent: 'space-between',
                        alignItems: 'center',
                        gap: '8px'
                    }}>
                        <span className="label-small" style={{ marginBottom: 0, fontSize: '0.9rem', letterSpacing: '2px' }}>REGEN</span>
                        <span className="value-display" style={{
                            fontSize: '1.3rem',
                            fontWeight: 'bold',
                            lineHeight: 1,
                            color: regenEnabled ? 'var(--fg-primary)' : 'var(--fg-muted)'
                        }}>
                            {regenEnabled === null || regenEnabled === undefined
                                ? '—'
                                : regenEnabled ? 'ON' : 'OFF'}
                        </span>
                    </div>

                    {/* MODE pill — which VCU params table is active. Burnt
                        orange matches TC: same "armed/active" pattern. Shows
                        the firmware-picked event (ACCEL/SKID/AUTOX/ENDUR);
                        the driver can't change it here, it's set by which
                        params header was compiled into the VCU. */}
                    <div style={{
                        height: '26px',
                        padding: '0 14px',
                        borderRadius: '5px',
                        border: `2px solid ${eventModeActive ? '#BF5700' : 'var(--card-border-hover)'}`,
                        background: eventModeActive ? 'rgba(191,87,0,0.20)' : 'var(--card-bg)',
                        display: 'flex',
                        justifyContent: 'space-between',
                        alignItems: 'center',
                        gap: '8px'
                    }}>
                        <span className="label-small" style={{ marginBottom: 0, fontSize: '0.9rem', letterSpacing: '2px' }}>MODE</span>
                        <span className="value-display" style={{
                            fontSize: '1.3rem',
                            fontWeight: 'bold',
                            lineHeight: 1,
                            color: eventModeActive ? 'var(--fg-primary)' : 'var(--fg-muted)'
                        }}>
                            {eventModeText ?? '—'}
                        </span>
                    </div>
                </div>

                {/* Right cluster: GEAR + HV, mirroring TC/REGEN on the
                    left. Same pill format (26px tall, label left + value
                    right). Replaces the old System OK widget which was
                    always-OK / dead info. TODO: negContactor=0 → shutdown
                    open, dash should route to ScreenTwo entirely. */}
                <div style={{
                    position: 'absolute',
                    right: '14px',
                    top: '50%',
                    transform: 'translateY(-50%)',
                    width: '160px',
                    display: 'flex',
                    flexDirection: 'column',
                    gap: '4px'
                }}>
                    {/* GEAR pill */}
                    <div style={{
                        height: '26px',
                        padding: '0 14px',
                        borderRadius: '5px',
                        border: `2px solid ${prndl === 'D' ? '#00CC66' : 'var(--card-border-hover)'}`,
                        background: prndl === 'D' ? 'rgba(0,204,102,0.20)' : 'var(--card-bg)',
                        display: 'flex',
                        justifyContent: 'space-between',
                        alignItems: 'center',
                        gap: '8px'
                    }}>
                        <span className="label-small" style={{ marginBottom: 0, fontSize: '0.9rem', letterSpacing: '2px' }}>GEAR</span>
                        <span className="value-display" style={{
                            fontSize: '1.3rem',
                            fontWeight: 'bold',
                            lineHeight: 1,
                            color: prndl === 'D' ? 'var(--fg-primary)' : 'var(--fg-muted)'
                        }}>
                            {prndl ?? '—'}
                        </span>
                    </div>

                    {/* HV pill — amber border/bg when posContactor=1
                        (energized). Whole pill blinks 1Hz when negContactor
                        is closed but posContactor is not yet engaged
                        (driver wait / precharge sequence). */}
                    <div style={{
                        height: '26px',
                        padding: '0 14px',
                        borderRadius: '5px',
                        border: `2px solid ${posContactor ? '#FFD700' : 'var(--card-border-hover)'}`,
                        background: posContactor ? 'rgba(255,215,0,0.20)' : 'var(--card-bg)',
                        display: 'flex',
                        justifyContent: 'space-between',
                        alignItems: 'center',
                        gap: '8px',
                        animation: !posContactor && negContactor
                            ? 'hv-precharge-blink 1s steps(1, end) infinite'
                            : 'none'
                    }}>
                        <span className="label-small" style={{ marginBottom: 0, fontSize: '0.9rem', letterSpacing: '2px' }}>HV</span>
                        <span className="value-display" style={{
                            fontSize: '1.3rem',
                            fontWeight: 'bold',
                            lineHeight: 1,
                            color: posContactor ? 'var(--fg-primary)' : 'var(--fg-muted)'
                        }}>
                            {posContactor === null
                                ? '—'
                                : posContactor
                                    ? 'ON'
                                    : negContactor
                                        ? 'WAIT'
                                        : 'OFF'}
                        </span>
                    </div>
                </div>

                {/* Energy Delta (Center) — label stacked above value so the
                    label can never overlap the digits on tight readouts. */}
                <div style={{
                    position: 'absolute',
                    left: '50%',
                    top: '50%',
                    transform: 'translate(-50%, -50%)',
                    zIndex: 100,
                    height: '72px',
                    width: '360px'
                }}>
                    {/* Title: top center */}
                    <div className="label-small" style={{
                        fontSize: '0.75rem',
                        position: 'absolute',
                        top: '2px',
                        left: '50%',
                        transform: 'translateX(-50%)',
                        marginBottom: 0,
                        whiteSpace: 'nowrap'
                    }}>
                        Energy Delta
                    </div>

                    {/* Value: Decimal Dead Center, anchored toward the bottom */}
                    {energyDelta !== null && energyDelta !== undefined ? (
                        <div style={{ position: 'relative', width: '100%', height: '100%', left: 0, top: 0 }}>
                            {/* Decimal Point - Dead Center */}
                            <div style={{
                                position: 'absolute',
                                left: '50%',
                                top: 'calc(50% + 10px)',
                                transform: 'translate(-50%, -50%)',
                                fontSize: '3rem',
                                fontWeight: 'bold',
                                lineHeight: 1,
                                color: getDeltaColor(energyDelta, true),
                                width: '20px',
                                textAlign: 'center'
                            }}>.</div>

                            {/* Integer Part */}
                            <div style={{
                                position: 'absolute',
                                right: '50%',
                                top: 'calc(50% + 10px)',
                                transform: 'translateY(-50%)',
                                fontSize: '3rem',
                                fontWeight: 'bold',
                                lineHeight: 1,
                                color: getDeltaColor(energyDelta, true),
                                marginRight: '10px',
                                textAlign: 'right',
                                whiteSpace: 'nowrap'
                            }}>
                                {energyDelta > 0 ? "+" : energyDelta < 0 ? "-" : ""}{Math.floor(Math.abs(energyDelta))}
                            </div>

                            {/* Fraction Part */}
                            <div style={{
                                position: 'absolute',
                                left: '50%',
                                top: 'calc(50% + 10px)',
                                transform: 'translateY(-50%)',
                                fontSize: '3rem',
                                fontWeight: 'bold',
                                lineHeight: 1,
                                color: getDeltaColor(energyDelta, true),
                                marginLeft: '10px',
                                textAlign: 'left',
                                whiteSpace: 'nowrap'
                            }}>
                                {Math.abs(energyDelta).toFixed(1).split('.')[1]} <span style={{fontSize: '1.5rem', marginLeft: '5px', verticalAlign: 'middle', color: 'var(--fg-muted)'}}>Wh</span>
                            </div>
                        </div>
                    ) : (
                        <div style={{ position: 'absolute', left: '50%', top: 'calc(50% + 10px)', transform: 'translate(-50%, -50%)', fontSize: '3rem', fontWeight: 'bold', color: 'var(--fg-muted)' }}>--</div>
                    )}
                </div>

                {/* Connectivity — right-anchored so its right edge sits
                    just before the GEAR/HV cluster (which starts at
                    right=174). Overlaps the Energy Delta container on
                    the left but Energy's actual digits are centered
                    narrowly inside the 360-wide container so the
                    rendered overlap is minimal. zIndex bumped so the
                    icons sit above. */}
                <div style={{
                    position: 'absolute',
                    right: '215px',
                    top: '50%',
                    transform: 'translateY(-50%)',
                    zIndex: 110
                }}>
                    <ConnectivityIndicator />
                </div>
            </div>

            {/* Full-screen lap card — pops on each lapTrigger crossing and
                clears itself after a few seconds (LAP_CARD_MS in the hook).
                Shows the just-finished lap's time and net energy so the driver
                gets a clear glance as they cross start/finish. */}
            {pacing.lapCard && (() => {
                // If trackside sent a custom lap-card layout, render it; otherwise
                // fall back to the built-in card. validateLapCardLayout returns null
                // for a missing/malformed layout, so the driver screen never blanks.
                const customLayout = validateLapCardLayout(data?.layout);
                if (customLayout && customLayout.widgets.length) {
                    const ctx = {
                        lapCard: pacing.lapCard,
                        pacing: data?.pacing,
                        can: data?.can,
                        mqtt: data?.mqtt,
                    };
                    return (
                        <div style={{ position: 'absolute', inset: 0, zIndex: 1000 }}>
                            <LapCardRenderer layout={customLayout} data={ctx} scale={1} theme={dashTheme} />
                        </div>
                    );
                }
                return (
                    <div style={{
                        position: 'absolute',
                        inset: 0,
                        zIndex: 1000,
                        background: dashTheme === 'light' ? 'rgba(244,241,237,0.94)' : 'rgba(8,8,10,0.92)',
                        backdropFilter: 'blur(6px)',
                        display: 'flex',
                        flexDirection: 'column',
                        alignItems: 'center',
                        justifyContent: 'center',
                        gap: '8px'
                    }}>
                        <div className="label-small" style={{ fontSize: '1.4rem', letterSpacing: '8px', color: '#BF5700' }}>
                            LAP {pacing.lapCard.lapNumber}
                        </div>
                        <div className="value-display" style={{ fontSize: '7rem', fontWeight: 'bold', lineHeight: 0.9 }}>
                            {fmtLapTime(pacing.lapCard.timeS)}
                        </div>
                        <div className="value-display" style={{ fontSize: '3rem', fontWeight: 'bold', color: 'var(--fg-secondary)', lineHeight: 1 }}>
                            {Math.round(pacing.lapCard.energyWh)}
                            <span className="label-small" style={{ fontSize: '1.1rem', marginLeft: '8px' }}>Wh / LAP</span>
                        </div>
                    </div>
                );
            })()}
        </div>
    );
};

export default ScreenOne;
