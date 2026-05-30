import React, { useEffect, useRef, useState } from 'react';
import ConnectivityIndicator from '../components/ConnectivityIndicator';
import { useDash } from '../context/DashContext';
import './ScreenOne.css';

// TEMP (driveday): 5-second time-series buffer of the value currently
// in CanData.speed (which dashd is sourcing from steer_col_angle for
// this test). Drop the whole STEER_* block + the rendered SVG below
// when reverting.
const STEER_HISTORY_SECONDS = 5;
const STEER_LOOKAHEAD_SECONDS = 5; // refs project this far past "now"
const STEER_SAMPLE_HZ = 30; // dashd WS rate; oversample is harmless
const STEER_MAX_SAMPLES = STEER_HISTORY_SECONDS * STEER_SAMPLE_HZ;
const STEER_TOTAL_SECONDS = STEER_HISTORY_SECONDS + STEER_LOOKAHEAD_SECONDS;
const STEER_VIEWBOX_WIDTH = STEER_TOTAL_SECONDS * STEER_SAMPLE_HZ;
const STEER_NOW_X = STEER_HISTORY_SECONDS * STEER_SAMPLE_HZ; // "now" tick mark
// Visual y-axis range depends on which reference set is showing. Switch
// at runtime via dashd: `echo ramp > /tmp/dash_chart_mode` (or "sine").
const STEER_SINE_AMPLITUDE_DEG = 10;
const STEER_RAMP_AMPLITUDE_DEG = 45;
const STEER_RAMP_PERIOD_S = 10;
// Ranges include a small margin past the reference amplitudes.
const SINE_CHART_MIN = -15;
const SINE_CHART_MAX = 15;
const RAMP_CHART_MIN = -5;
const RAMP_CHART_MAX = 50;

// Screen One: Main Dashboard (Modern EV Style)
// Resolution: 800 x 480

const ScreenOne: React.FC = () => {
    const { data } = useDash();

    // TEMP: rolling timestamped buffer of the speed-field-as-steering.
    // Why timestamped + wall-clock driven (not data.seq driven): if speed
    // ever momentarily becomes null while seq keeps ticking, a seq-driven
    // buffer would stop advancing while the references (which use
    // Date.now() every render) keep scrolling — so the orange trace
    // appears frozen against moving refs. Driving on setInterval keeps
    // the trace ticking even across brief CAN/WS hiccups.
    const [steerHistory, setSteerHistory] = useState<Array<{ t: number; v: number }>>([]);
    const latestSampleRef = useRef<number | null>(null);
    useEffect(() => {
        const v = data?.can.speed;
        if (typeof v === 'number') latestSampleRef.current = v;
    }, [data?.seq, data?.can.speed]);
    useEffect(() => {
        const id = setInterval(() => {
            const t = Date.now() / 1000;
            const cutoff = t - STEER_HISTORY_SECONDS;
            const v = latestSampleRef.current;
            setSteerHistory(prev => {
                const trimmed = prev[0] && prev[0].t < cutoff
                    ? prev.filter(p => p.t >= cutoff)
                    : prev;
                if (typeof v !== 'number') return trimmed === prev ? prev : trimmed;
                return [...trimmed, { t, v }];
            });
        }, 1000 / STEER_SAMPLE_HZ);
        return () => clearInterval(id);
    }, []);

    // Extract values with null fallback
    const speed = data?.can.speed;
    const power = data?.can.power;
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

            {/* Inner square: 2-row grid spanning between sidebars and trays.
                Top row splits into Speed (left) + Lap-stuff table (right).
                Bottom row hosts the big bidirectional power bar + readouts. */}
            <div style={{
                position: 'absolute',
                top: '60px',
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
                    {/* TEMP: dashd is currently sending steer_col_angle in
                        place of speed for driveday testing. Restore "MPH"
                        when dashd swaps back to motor_speed-derived. */}
                    <div className="label-small" style={{ fontSize: '1.2rem', letterSpacing: '4px', marginTop: '4px' }}>DEG</div>
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
                    {/* Power readout (left) + Energy laps remaining (right) */}
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'baseline', width: '95%' }}>
                        <div className="value-display" style={{ fontSize: '2.5rem', fontWeight: 'bold', lineHeight: 1 }}>
                            {fmt(power)}
                            <span className="label-small" style={{ fontSize: '0.9rem', marginLeft: '6px' }}>kW</span>
                        </div>
                        <div className="value-display" style={{ fontSize: '2.5rem', fontWeight: 'bold', color: 'var(--fg-secondary)', lineHeight: 1 }}>
                            <span className="label-small" style={{ fontSize: '0.9rem', marginRight: '6px' }}>BB</span>
                            {brakeBias !== null && brakeBias !== undefined ? brakeBias.toFixed(0) : '--'}
                            <span className="label-small" style={{ fontSize: '0.9rem', marginLeft: '3px' }}>%</span>
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

            {/* TEMP: live steering-angle trace, 5s rolling window.
                Overlays the lower inner-grid (power bar + kW/BB row).
                Drop this whole block when reverting. */}
            <div style={{
                position: 'absolute',
                left: '100px',
                right: '100px',
                bottom: '90px',
                height: '160px',
                background: 'var(--card-bg)',
                backdropFilter: 'blur(8px)',
                border: '1px solid var(--card-border)',
                borderRadius: '8px',
                zIndex: 95,
                overflow: 'hidden',
            }}>
                <div className="label-small" style={{
                    position: 'absolute',
                    top: '4px',
                    left: '10px',
                    margin: 0,
                    fontSize: '0.7rem',
                    letterSpacing: '2px',
                }}>
                    {data?.chartMode === 'ramp'
                        ? <>STEER (DEG) — 5s back · 5s ahead · <span style={{ color: '#CC00AA' }}>RAMP</span> 0→45° / 10s · range −5°..50°</>
                        : <>STEER (DEG) — 5s back · 5s ahead · <span style={{ color: '#000000' }}>0.5 Hz</span> · <span style={{ color: '#0066FF' }}>1 Hz</span> ±10°</>
                    }
                </div>
                {(() => {
                    // Switch which reference set is drawn + the chart's
                    // vertical range to fit it. Mode is set by writing
                    // "sine" or "ramp" to /tmp/dash_chart_mode on BEVO.
                    const mode = data?.chartMode === 'ramp' ? 'ramp' : 'sine';
                    const [chartMin, chartMax] = mode === 'ramp'
                        ? [RAMP_CHART_MIN, RAMP_CHART_MAX]
                        : [SINE_CHART_MIN, SINE_CHART_MAX];
                    // Linear deg → viewBox y in [5, 195] (small padding off the rails).
                    const degToY = (deg: number): number => {
                        const c = Math.max(chartMin, Math.min(chartMax, deg));
                        return 5 + ((chartMax - c) / (chartMax - chartMin)) * 190;
                    };
                    const nowSec = Date.now() / 1000;
                    // x ∈ [0, STEER_VIEWBOX_WIDTH] maps to time
                    // [nowSec - HISTORY, nowSec + LOOKAHEAD]. "now" sits at
                    // STEER_NOW_X (the middle when history==lookahead).
                    const timeForX = (x: number) =>
                        nowSec - STEER_HISTORY_SECONDS + x / STEER_SAMPLE_HZ;
                    const xForTime = (t: number) =>
                        (t - (nowSec - STEER_HISTORY_SECONDS)) * STEER_SAMPLE_HZ;
                    const sinePoints = (freqHz: number) =>
                        Array.from({ length: STEER_VIEWBOX_WIDTH + 1 }, (_, x) => {
                            const t = timeForX(x);
                            const deg = STEER_SINE_AMPLITUDE_DEG * Math.sin(2 * Math.PI * freqHz * t);
                            return `${x},${degToY(deg)}`;
                        }).join(' ');
                    // Sawtooth ramp: linear 0 → STEER_RAMP_AMPLITUDE_DEG over
                    // STEER_RAMP_PERIOD_S, then snaps back to 0.
                    const rampPoints = Array.from(
                        { length: STEER_VIEWBOX_WIDTH + 1 },
                        (_, x) => {
                            const t = timeForX(x);
                            const phase = ((t % STEER_RAMP_PERIOD_S) + STEER_RAMP_PERIOD_S) % STEER_RAMP_PERIOD_S;
                            const deg = (phase / STEER_RAMP_PERIOD_S) * STEER_RAMP_AMPLITUDE_DEG;
                            return `${x},${degToY(deg)}`;
                        },
                    ).join(' ');
                    return (
                        <svg
                            viewBox={`0 0 ${STEER_VIEWBOX_WIDTH} 200`}
                            preserveAspectRatio="none"
                            style={{ width: '100%', height: '100%', display: 'block' }}
                        >
                            {/* zero line (only when 0 is inside the visible range) */}
                            {chartMin <= 0 && chartMax >= 0 && (
                                <line
                                    x1={0} y1={degToY(0)}
                                    x2={STEER_VIEWBOX_WIDTH} y2={degToY(0)}
                                    stroke="var(--card-border)"
                                    strokeWidth={1}
                                    vectorEffect="non-scaling-stroke"
                                />
                            )}
                            {/* amplitude bound markers for the active mode */}
                            {mode === 'sine' && (
                                <>
                                    <line
                                        x1={0} y1={degToY(STEER_SINE_AMPLITUDE_DEG)}
                                        x2={STEER_VIEWBOX_WIDTH} y2={degToY(STEER_SINE_AMPLITUDE_DEG)}
                                        stroke="var(--card-border)"
                                        strokeWidth={0.5}
                                        strokeDasharray="2,4"
                                        vectorEffect="non-scaling-stroke"
                                    />
                                    <line
                                        x1={0} y1={degToY(-STEER_SINE_AMPLITUDE_DEG)}
                                        x2={STEER_VIEWBOX_WIDTH} y2={degToY(-STEER_SINE_AMPLITUDE_DEG)}
                                        stroke="var(--card-border)"
                                        strokeWidth={0.5}
                                        strokeDasharray="2,4"
                                        vectorEffect="non-scaling-stroke"
                                    />
                                </>
                            )}
                            {mode === 'ramp' && (
                                <line
                                    x1={0} y1={degToY(STEER_RAMP_AMPLITUDE_DEG)}
                                    x2={STEER_VIEWBOX_WIDTH} y2={degToY(STEER_RAMP_AMPLITUDE_DEG)}
                                    stroke="var(--card-border)"
                                    strokeWidth={0.5}
                                    strokeDasharray="2,4"
                                    vectorEffect="non-scaling-stroke"
                                />
                            )}
                            {/* "now" marker — trace ends here, refs continue right */}
                            <line
                                x1={STEER_NOW_X} y1={0}
                                x2={STEER_NOW_X} y2={200}
                                stroke="var(--card-border)"
                                strokeWidth={1}
                                strokeDasharray="4,4"
                                vectorEffect="non-scaling-stroke"
                            />

                            {/* References — only the active mode's set */}
                            {mode === 'sine' && (
                                <>
                                    <polyline
                                        points={sinePoints(0.5)}
                                        fill="none"
                                        stroke="#000000"
                                        strokeWidth={2}
                                        vectorEffect="non-scaling-stroke"
                                    />
                                    <polyline
                                        points={sinePoints(1.0)}
                                        fill="none"
                                        stroke="#0066FF"
                                        strokeWidth={2}
                                        vectorEffect="non-scaling-stroke"
                                    />
                                </>
                            )}
                            {mode === 'ramp' && (
                                <polyline
                                    points={rampPoints}
                                    fill="none"
                                    stroke="#CC00AA"
                                    strokeWidth={2}
                                    vectorEffect="non-scaling-stroke"
                                />
                            )}
                            {/* Driver trace — positioned by sample time so it
                                ends at the "now" marker; refs project to the
                                right of it. */}
                            <polyline
                                points={steerHistory
                                    .map(({ t, v }) => `${xForTime(t)},${degToY(v)}`)
                                    .join(' ')}
                                fill="none"
                                stroke="var(--brand)"
                                strokeWidth={2}
                                vectorEffect="non-scaling-stroke"
                            />
                        </svg>
                    );
                })()}
            </div>
        </div>
    );
};

export default ScreenOne;
