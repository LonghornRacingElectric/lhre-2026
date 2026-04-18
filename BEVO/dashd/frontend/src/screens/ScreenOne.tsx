import React from 'react';
import { Container, Row, Col } from 'react-bootstrap';
import RadialGauge from '../components/RadialGauge';
import VerticalGauge from '../components/VerticalGauge';
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
    const odometer = data?.can.odometer;
    const signalStrength = data?.can.signalStrength;
    const shutdown = data?.can.shutdown;
    const lapDelta = data?.mqtt.lapDelta;
    const energyDelta = data?.mqtt.energyDelta;
    const lapsRemaining = data?.mqtt.lapsRemaining;

    // Derived: telemetry status (all mqtt null = disconnected)
    const telemetryStatus = lapDelta !== null || energyDelta !== null || lapsRemaining !== null;

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

    const BRAND_COLOR = "#BF5700"; // Burnt Orange

    // Safe numeric values for gauges (default to 0 when null)
    const safeSpeed = speed ?? 0;
    const safePower = power ?? 0;
    const safeCharge = charge ?? 0;
    const safeTemp = temp ?? 0;
    const safeSignal = signalStrength ?? 0;

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
                borderRadius: '0 0 12px 12px',
                background: 'rgba(255, 255, 255, 0.05)',
                backdropFilter: 'blur(12px)',
                border: '1px solid rgba(255, 255, 255, 0.1)',
                borderTop: 'none',
                boxShadow: '0 4px 20px rgba(0,0,0,0.5)'
            }}>
                {/* Left: Connectivity */}
                <div style={{ position: 'absolute', left: '30px', top: '50%', transform: 'translateY(-50%)', display: 'flex', alignItems: 'center', gap: '8px' }}>
                    {/* Telemetry Status Light */}
                    <div style={{
                        width: '10px',
                        height: '10px',
                        borderRadius: '50%',
                        backgroundColor: telemetryStatus ? '#00FF66' : '#FF3333',
                        boxShadow: telemetryStatus ? '0 0 8px #00FF66' : '0 0 8px #FF3333'
                    }} />
                    <div className="label-small" style={{ fontSize: '0.8rem', marginBottom: 0, color: '#aaa' }}>5G</div>
                    <div style={{ display: 'flex', alignItems: 'flex-end', gap: '3px', height: '14px' }}>
                        {[1, 2, 3, 4].map(bar => (
                            <div key={bar} style={{
                                width: '4px',
                                height: `${bar * 25}%`,
                                backgroundColor: bar <= safeSignal ? '#fff' : 'rgba(255,255,255,0.2)',
                                borderRadius: '1px'
                            }} />
                        ))}
                    </div>
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

                {/* Right: Laps Remaining */}
                <div style={{ position: 'absolute', right: '30px', top: '50%', transform: 'translateY(-50%)', textAlign: 'right' }}>
                    <div className="label-small" style={{ marginBottom: 0 }}>Laps Rem.</div>
                    <div className="value-display" style={{ fontSize: '1.5rem', fontWeight: 'bold', lineHeight: 1 }}>
                        {lapsRemaining !== null && lapsRemaining !== undefined ? lapsRemaining.toFixed(1) : "--"}
                    </div>
                </div>
            </div>

            <Container fluid style={{ height: '100%' }}>
                <Row style={{ height: '100%' }}>

                    {/* 1. Left Column: Temp */}
                    <Col xs={2} className="h-100-flex align-items-center" style={{ paddingTop: '70px', paddingBottom: '90px' }}>
                        <div className="dash-card d-flex flex-column align-items-center" style={{ width: '100%', height: '100%', justifyContent: 'center', border: 'none' }}>
                            <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '2px' }}>
                                <div className="label-small text-center" style={{ fontSize: '1rem', letterSpacing: '2px' }}>TEMP</div>
                                <VerticalGauge
                                    value={safeTemp}
                                    min={0}
                                    max={100}
                                    label=""
                                    color={safeTemp > 80 ? "#ff0000" : BRAND_COLOR}
                                    height={220}
                                    width={40}
                                />
                                <div className="text-center value-display" style={{ fontSize: '2rem', fontWeight: 'bold' }}>
                                    {temp !== null && temp !== undefined ? Math.round(temp * 9/5 + 32) : "--"}
                                    <span style={{ fontSize: '1rem', marginLeft: '5px', color: '#888' }}>°F</span>
                                </div>
                            </div>
                        </div>
                    </Col>

                    {/* 2. Center Cluster: Speed & Power */}
                    <Col xs={8} className="h-100-flex" style={{ paddingTop: '60px', paddingBottom: '80px' }}>
                        <Row className="h-100">
                            {/* Speed Section */}
                            <Col xs={12} className="d-flex flex-column align-items-center justify-content-center">
                                <div style={{ position: 'relative' }}>
                                    <RadialGauge
                                        value={safeSpeed}
                                        min={0}
                                        max={100}
                                        label=""
                                        size={340}
                                        color={BRAND_COLOR}
                                        numTicks={10}
                                        strokeWidth={18}
                                        className="glow-orange"
                                        showValueText={false}
                                    />
                                    {/* Center Text Overlay */}
                                    <div style={{ position: 'absolute', top: '40%', left: '50%', transform: 'translate(-50%, -50%)', textAlign: 'center' }}>
                                        <div className="value-display" style={{ fontSize: '5rem', fontWeight: 'bold', lineHeight: 1 }}>{fmt(speed)}</div>
                                        <div className="label-small" style={{ fontSize: '0.9rem' }}>MPH</div>
                                    </div>

                                    {/* Horizontal Energy Bar */}
                                    <div style={{ position: 'absolute', top: '60%', left: '50%', transform: 'translateX(-50%)', width: '180px', textAlign: 'center' }}>
                                        <div style={{ width: '100%', height: '8px', background: 'rgba(255,255,255,0.1)', borderRadius: '4px', position: 'relative', overflow: 'hidden' }}>
                                            {/* Center Marker */}
                                            <div style={{ position: 'absolute', left: '50%', top: 0, bottom: 0, width: '2px', background: 'rgba(255,255,255,0.3)', transform: 'translateX(-50%)', zIndex: 2 }} />

                                            {/* Fill */}
                                            <div style={{
                                                position: 'absolute',
                                                top: 0, bottom: 0,
                                                left: safePower < 0 ? `${50 - (Math.min(Math.abs(safePower), 80)/80)*50}%` : '50%',
                                                width: `${(Math.min(Math.abs(safePower), 80)/80)*50}%`,
                                                background: safePower < 0 ? 'linear-gradient(to right, #00CC00, #00FF66)' : 'linear-gradient(to left, #FF0000, #BF5700)',
                                                transition: 'all 0.1s linear'
                                            }} />
                                        </div>
                                        <div className="label-small" style={{ fontSize: '0.7rem', marginTop: '4px', color: '#888' }}>
                                            {/* Math.round(power)} kW */}
                                        </div>
                                    </div>

                                    {/* kW Digital Readout */}
                                    <div style={{ position: 'absolute', top: '75%', left: '50%', transform: 'translate(-50%, -50%)', textAlign: 'center' }}>
                                        <div className="value-display" style={{ fontSize: '2.5rem', fontWeight: 'bold', lineHeight: 1, color: '#FFF' }}>
                                            {fmt(power)}
                                        </div>
                                        <div className="label-small" style={{ fontSize: '0.9rem' }}>kW</div>
                                    </div>
                                </div>

                            </Col>

                            {/* Power Section (Temporarily removed)
                            <Col xs={6} className="d-flex flex-column align-items-center justify-content-center">
                                <div style={{ position: 'relative' }}>
                                    <RadialGauge
                                        value={safePower}
                                        min={-80}
                                        max={80}
                                        label=""
                                        size={240}
                                        mode="bidirectional"
                                        numTicks={8}
                                        strokeWidth={10}
                                        className="glow-gradient"
                                        showValueText={false}
                                    />
                                     <div style={{ position: 'absolute', top: '50%', left: '50%', transform: 'translate(-50%, -50%)', textAlign: 'center' }}>
                                        <div className="value-display" style={{ fontSize: '3rem', fontWeight: 'bold', lineHeight: 1 }}>{fmt(power)}</div>
                                        <div className="label-small" style={{ fontSize: '0.9rem' }}>kW</div>
                                    </div>
                                </div>
                            </Col>
                            */}
                        </Row>
                    </Col>

                    {/* 4. Right Column: Charge & Laps */}
                    <Col xs={2} className="h-100-flex align-items-center" style={{ paddingTop: '70px', paddingBottom: '90px' }}>
                        <div className="dash-card d-flex flex-column align-items-center" style={{ width: '100%', height: '100%', justifyContent: 'center', border: 'none' }}>
                            <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '2px' }}>
                                <div className="label-small text-center" style={{ fontSize: '1rem', letterSpacing: '2px' }}>SOC</div>
                                <VerticalGauge
                                    value={safeCharge}
                                    min={0}
                                    max={100}
                                    label=""
                                    color="#FFD700"
                                    height={220}
                                    width={40}
                                />
                                <div className="text-center value-display" style={{ fontSize: '2rem', fontWeight: 'bold' }}>
                                    {fmt(charge)}
                                    <span style={{ fontSize: '1rem', marginLeft: '5px', color: '#888' }}>%</span>
                                </div>
                            </div>
                        </div>
                    </Col>

                </Row>
            </Container>

            {/* Bottom Panel - Energy Delta & Odometer */}
            <div className="dash-card" style={{
                position: 'absolute',
                bottom: '0',
                left: '0',
                zIndex: 100,
                height: '80px',
                width: '100%',
                borderRadius: '12px 12px 0 0',
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
                {/* Odometer (Right) */}
                <div style={{ position: 'absolute', right: '30px', top: '50%', transform: 'translateY(-50%)', display: 'flex', flexDirection: 'column', alignItems: 'center' }}>
                    <div className="label-small" style={{ fontSize: '0.75rem', marginBottom: 0 }}>Odometer</div>
                    <div className="value-display" style={{ fontSize: '1.2rem', fontWeight: 'bold' }}>
                        {odometer !== null && odometer !== undefined ? odometer.toFixed(1) : "--"} <span className="unit-label">miles</span>
                    </div>
                </div>
            </div>
        </div>
    );
};

export default ScreenOne;
