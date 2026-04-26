import React from 'react';
import ConnectivityIndicator from '../components/ConnectivityIndicator';
import { useDash } from '../context/DashContext';
import './ScreenOne.css';

// Screen One: Main Dashboard (Modern EV Style)
// Resolution: 800 x 480

const ScreenOne: React.FC = () => {
    const { data } = useDash();

    // Extract values with null fallback
    const speed = data?.can.speed;
    const power = data?.can.power;
    const charge = data?.can.soc;
    const temp = data?.can.temperature;
    const shutdown = data?.can.shutdown;
    const lapDelta = data?.mqtt.lapDelta;
    const energyDelta = data?.mqtt.energyDelta;
    const lapsRemaining = data?.mqtt.lapsRemaining;

    // ----- Driver-thread proposals: stubbed null until backend wires up -----
    // (Cast through `as number | null` so TS doesn't collapse `!== null`
    // checks to `never` on the literal-null assignment.)
    // BACKEND TODO: needs MqttData.lapsRemainingEnergy (off-car prediction
    // from SOC + average consumption — see BEVO/dashd/MQTT_CONTRACT.md).
    const lapsRemainingEnergy = null as number | null;
    // BACKEND TODO: needs MqttData.bestLapTime / .lastLapTime (off-car
    // computed) and on-car timer for currentLapTime (driven by lap signal).
    const bestLapTime = null as number | null;
    const lastLapTime = null as number | null;
    const currentLapTime = null as number | null;
    // BACKEND TODO: needs CanData.brakeBias derived from front/rear brake
    // pressures. Bouncy on Angelique — needs low-pass filter and should
    // probably gate display by brake pressure > threshold.
    const brakeBias = null as number | null;

    // Derived: system status from shutdown circuit (any false = FAULT)
    const systemOk = shutdown ? shutdown.every(Boolean) : null;

    // Derived: alerts from temperature thresholds
    const alerts: string[] = [];
    if (temp !== null && temp !== undefined && temp > 80) {
        alerts.push("High Battery Temp");
    }
    if (systemOk === false) {
        alerts.push("Shutdown Circuit Fault");
    }

    // -------------------------------------------------------------------------
    // RENDER HELPERS
    // -------------------------------------------------------------------------

    const getDeltaColor = (val: number | null | undefined, inverse: boolean = false) => {
        if (val === null || val === undefined || val === 0) return "#888";
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
                background: 'rgba(255, 255, 255, 0.05)',
                backdropFilter: 'blur(12px)',
                border: '1px solid rgba(255, 255, 255, 0.1)',
                borderTop: 'none',
                boxShadow: '0 4px 20px rgba(0,0,0,0.5)'
            }}>
                {/* Left: Connectivity */}
                <div style={{ position: 'absolute', left: '30px', top: '50%', transform: 'translateY(-50%)' }}>
                    <ConnectivityIndicator />
                </div>

                {/* Center: Lap Delta */}
                <div style={{
                    position: 'absolute',
                    left: '50%',
                    top: '50%',
                    transform: 'translate(-50%, -50%)',
                    zIndex: 100,
                    height: '100%',
                    width: '500px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center'
                }}>
                    {/* Title: Fixed to left */}
                    <div className="label-small" style={{
                        fontSize: '1rem',
                        position: 'absolute',
                        left: '30px',
                        marginBottom: 0
                    }}>
                        Lap Delta
                    </div>

                    {/* Value: Decimal Dead Center */}
                    {lapDelta !== null && lapDelta !== undefined ? (
                        <div style={{ position: 'relative', width: '100%', height: '100%' }}>
                            {/* Decimal Point - Dead Center */}
                            <div style={{
                                position: 'absolute',
                                left: '50%',
                                top: '50%',
                                transform: 'translate(-50%, -50%)',
                                fontSize: '3rem',
                                fontWeight: 'bold',
                                lineHeight: 1,
                                color: getDeltaColor(lapDelta),
                                width: '20px',
                                textAlign: 'center'
                            }}>.</div>

                            {/* Integer Part */}
                            <div style={{
                                position: 'absolute',
                                right: '50%',
                                top: '50%',
                                transform: 'translateY(-50%)',
                                fontSize: '3rem',
                                fontWeight: 'bold',
                                lineHeight: 1,
                                color: getDeltaColor(lapDelta),
                                marginRight: '10px',
                                textAlign: 'right',
                                whiteSpace: 'nowrap'
                            }}>
                                {lapDelta > 0 ? "+" : lapDelta < 0 ? "-" : ""}{Math.floor(Math.abs(lapDelta))}
                            </div>

                            {/* Fraction Part */}
                            <div style={{
                                position: 'absolute',
                                left: '50%',
                                top: '50%',
                                transform: 'translateY(-50%)',
                                fontSize: '3rem',
                                fontWeight: 'bold',
                                lineHeight: 1,
                                color: getDeltaColor(lapDelta),
                                marginLeft: '10px',
                                textAlign: 'left',
                                whiteSpace: 'nowrap'
                            }}>
                                {Math.abs(lapDelta).toFixed(2).split('.')[1]} <span style={{fontSize: '1.5rem', marginLeft: '5px', verticalAlign: 'middle', color: '#888'}}>s</span>
                            </div>
                        </div>
                    ) : (
                        <div style={{ fontSize: '3rem', fontWeight: 'bold', color: '#888' }}>--</div>
                    )}
                </div>

                {/* Right: Brake Bias — label sits left of the (larger) number */}
                <div style={{ position: 'absolute', right: '30px', top: '50%', transform: 'translateY(-50%)', display: 'flex', alignItems: 'baseline', gap: '10px' }}>
                    <span className="label-small" style={{ marginBottom: 0 }}>BB</span>
                    <span className="value-display" style={{ fontSize: '2.2rem', fontWeight: 'bold', lineHeight: 1 }}>
                        {brakeBias !== null && brakeBias !== undefined ? `${brakeBias.toFixed(0)}%` : '--'}
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
                background: 'rgba(255, 255, 255, 0.05)',
                backdropFilter: 'blur(12px)',
                border: '1px solid rgba(255, 255, 255, 0.1)',
                boxShadow: '0 4px 20px rgba(0,0,0,0.5)',
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
                    height: `calc((100% - 8px) * ${Math.min(Math.max(safeTemp, 0), 100) / 100})`,
                    background: safeTemp > 80 ? '#ff0000' : BRAND_COLOR,
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
                    color: '#fff',
                    textShadow: '0 1px 3px rgba(0,0,0,0.7)'
                }}>TEMP</div>

                <div className="text-center value-display" style={{
                    position: 'relative',
                    zIndex: 2,
                    marginBottom: '12px',
                    fontSize: '2.4rem',
                    fontWeight: 'bold',
                    lineHeight: 1,
                    color: '#fff',
                    textShadow: '0 1px 4px rgba(0,0,0,0.8)'
                }}>
                    {temp !== null && temp !== undefined ? Math.round(temp * 9/5 + 32) : "--"}
                    <span style={{ fontSize: '1rem', marginLeft: '4px', color: 'rgba(255,255,255,0.85)' }}>°F</span>
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
                background: 'rgba(255, 255, 255, 0.05)',
                backdropFilter: 'blur(12px)',
                border: '1px solid rgba(255, 255, 255, 0.1)',
                boxShadow: '0 4px 20px rgba(0,0,0,0.5)',
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
                    color: '#fff',
                    textShadow: '0 1px 3px rgba(0,0,0,0.7)'
                }}>SOC</div>

                <div className="text-center value-display" style={{
                    position: 'relative',
                    zIndex: 2,
                    marginBottom: '12px',
                    fontSize: '2.4rem',
                    fontWeight: 'bold',
                    lineHeight: 1,
                    color: '#fff',
                    textShadow: '0 1px 4px rgba(0,0,0,0.8)'
                }}>
                    {fmt(charge)}
                    <span style={{ fontSize: '1rem', marginLeft: '4px', color: 'rgba(255,255,255,0.85)' }}>%</span>
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
                gridTemplateColumns: '1fr 1fr',
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

                {/* TR: Lap-stuff table (2x2): BEST | LAST / CURR | NRG */}
                <div style={{
                    display: 'grid',
                    gridTemplateColumns: '1fr 1fr',
                    gridTemplateRows: '1fr 1fr',
                    gap: '8px',
                    padding: '4px'
                }}>
                    {[
                        { label: 'BEST', value: fmtLapTime(bestLapTime) },
                        { label: 'LAST', value: fmtLapTime(lastLapTime) },
                        { label: 'CURR', value: fmtLapTime(currentLapTime), highlight: true },
                        { label: 'SES', value: fmt(lapsRemaining, 1), suffix: 'laps' },
                    ].map((cell) => (
                        <div key={cell.label} style={{
                            display: 'flex',
                            flexDirection: 'column',
                            justifyContent: 'center',
                            alignItems: 'flex-start',
                            paddingLeft: '12px',
                            borderLeft: '2px solid rgba(255, 255, 255, 0.08)'
                        }}>
                            <div className="label-small" style={{
                                fontSize: '0.7rem',
                                letterSpacing: '2px',
                                marginBottom: '2px',
                                color: cell.highlight ? '#FFD700' : '#888'
                            }}>{cell.label}</div>
                            <div className="value-display" style={{
                                fontSize: '1.4rem',
                                fontWeight: 'bold',
                                lineHeight: 1,
                                color: cell.highlight ? '#FFD700' : '#fff'
                            }}>
                                {cell.value}
                                {cell.suffix && (
                                    <span style={{ fontSize: '0.7rem', marginLeft: '4px', color: '#888' }}>{cell.suffix}</span>
                                )}
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
                        <div className="value-display" style={{ fontSize: '1.5rem', fontWeight: 'bold', color: '#aaa' }}>
                            <span className="label-small" style={{ fontSize: '0.7rem', marginRight: '6px' }}>NRG</span>
                            {fmt(lapsRemainingEnergy, 1)}
                            <span className="label-small" style={{ fontSize: '0.7rem', marginLeft: '6px' }}>laps</span>
                        </div>
                    </div>

                    {/* Big power bar — 20% regen on the left, 80% drive on the right */}
                    <div style={{ width: '95%', height: '22px', background: 'rgba(255,255,255,0.08)', borderRadius: '4px', position: 'relative', overflow: 'hidden' }}>
                        {/* Zero marker (where regen meets drive) */}
                        <div style={{ position: 'absolute', left: `${REGEN_PCT}%`, top: 0, bottom: 0, width: '2px', background: 'rgba(255,255,255,0.3)', transform: 'translateX(-50%)', zIndex: 2 }} />
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
                background: 'rgba(255, 255, 255, 0.05)',
                backdropFilter: 'blur(12px)',
                border: '1px solid rgba(255, 255, 255, 0.1)',
                boxShadow: '0 4px 20px rgba(0,0,0,0.5)',
                padding: 0
            }}>
                {/* Left: System Status */}
                <div style={{ position: 'absolute', left: '0', top: '50%', transform: 'translateY(-50%)', textAlign: 'center', width: '120px' }}>
                    <div className="label-small" style={{ marginBottom: 0 }}>System</div>
                    {systemOk === null ? (
                        <div className="value-display" style={{ fontSize: '1.5rem', fontWeight: 'bold', color: '#888', lineHeight: 1 }}>--</div>
                    ) : systemOk ? (
                        <div className="value-display" style={{ fontSize: '1.5rem', fontWeight: 'bold', color: '#00FF66', textShadow: '0 0 5px rgba(0,255,100,0.5)', lineHeight: 1 }}>OK</div>
                    ) : (
                        <div className="value-display" style={{ fontSize: '1.5rem', fontWeight: 'bold', color: '#FF3333', textShadow: '0 0 5px rgba(255,50,50,0.5)', lineHeight: 1 }}>FAULT</div>
                    )}
                </div>

                {/* Vertical Divider for System Status */}
                <div style={{
                    position: 'absolute',
                    left: '120px',
                    top: '50%',
                    transform: 'translateY(-50%)',
                    height: '60%',
                    width: '1px',
                    background: 'rgba(255, 255, 255, 0.1)'
                }} />

                {/* Energy Delta (Center) */}
                <div style={{
                    position: 'absolute',
                    left: '50%',
                    top: '50%',
                    transform: 'translate(-50%, -50%)',
                    zIndex: 100,
                    height: '60px',
                    width: '500px'
                }}>
                    {/* Title: Fixed to left */}
                    <div className="label-small" style={{
                        fontSize: '1rem',
                        position: 'absolute',
                        left: '30px',
                        top: '50%',
                        transform: 'translateY(-50%)',
                        marginBottom: 0
                    }}>
                        Energy Delta
                    </div>

                    {/* Value: Decimal Dead Center */}
                    {energyDelta !== null && energyDelta !== undefined ? (
                        <div style={{ position: 'relative', width: '100%', height: '100%', left: 0, top: 0 }}>
                            {/* Decimal Point - Dead Center */}
                            <div style={{
                                position: 'absolute',
                                left: '50%',
                                top: '50%',
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
                                top: '50%',
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
                                top: '50%',
                                transform: 'translateY(-50%)',
                                fontSize: '3rem',
                                fontWeight: 'bold',
                                lineHeight: 1,
                                color: getDeltaColor(energyDelta, true),
                                marginLeft: '10px',
                                textAlign: 'left',
                                whiteSpace: 'nowrap'
                            }}>
                                {Math.abs(energyDelta).toFixed(1).split('.')[1]} <span style={{fontSize: '1.5rem', marginLeft: '5px', verticalAlign: 'middle', color: '#888'}}>Wh</span>
                            </div>
                        </div>
                    ) : (
                        <div style={{ position: 'absolute', left: '50%', top: '50%', transform: 'translate(-50%, -50%)', fontSize: '3rem', fontWeight: 'bold', color: '#888' }}>--</div>
                    )}
                </div>
            </div>
        </div>
    );
};

export default ScreenOne;
