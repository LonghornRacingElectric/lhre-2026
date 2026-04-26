import React from 'react';
import { Container, Row, Col } from 'react-bootstrap';
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
    const safePower = power ?? 0;
    const safeCharge = charge ?? 0;
    const safeTemp = temp ?? 0;

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

                {/* Right: Laps Remaining */}
                <div style={{ position: 'absolute', right: '30px', top: '50%', transform: 'translateY(-50%)', textAlign: 'right' }}>
                    <div className="label-small" style={{ marginBottom: 0 }}>Laps Rem.</div>
                    <div className="value-display" style={{ fontSize: '1.5rem', fontWeight: 'bold', lineHeight: 1 }}>
                        {lapsRemaining !== null && lapsRemaining !== undefined ? lapsRemaining.toFixed(1) : "--"}
                    </div>
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

            {/* Center Cluster: Speed (huge) + Power bar + kW readout */}
            <Container fluid style={{ height: '100%' }}>
                <Row style={{ height: '100%' }}>
                    <Col xs={12} className="h-100-flex" style={{ paddingTop: '60px', paddingBottom: '80px' }}>
                        <Row className="h-100">
                            <Col xs={12} className="d-flex flex-column align-items-center justify-content-center">
                                {/* Speed digit + MPH label */}
                                <div className="text-center" style={{ marginBottom: '24px' }}>
                                    <div className="value-display" style={{ fontSize: '8rem', fontWeight: 'bold', lineHeight: 1 }}>
                                        {fmt(speed)}
                                    </div>
                                    <div className="label-small" style={{ fontSize: '1rem', letterSpacing: '3px', marginTop: '4px' }}>MPH</div>
                                </div>

                                {/* Bidirectional power bar (regen left, drive right) */}
                                <div style={{ width: '260px', marginBottom: '12px' }}>
                                    <div style={{ width: '100%', height: '8px', background: 'rgba(255,255,255,0.1)', borderRadius: '4px', position: 'relative', overflow: 'hidden' }}>
                                        <div style={{ position: 'absolute', left: '50%', top: 0, bottom: 0, width: '2px', background: 'rgba(255,255,255,0.3)', transform: 'translateX(-50%)', zIndex: 2 }} />
                                        <div style={{
                                            position: 'absolute',
                                            top: 0, bottom: 0,
                                            left: safePower < 0 ? `${50 - (Math.min(Math.abs(safePower), 80)/80)*50}%` : '50%',
                                            width: `${(Math.min(Math.abs(safePower), 80)/80)*50}%`,
                                            background: safePower < 0 ? 'linear-gradient(to right, #00CC00, #00FF66)' : 'linear-gradient(to left, #FF0000, #BF5700)',
                                            transition: 'all 0.1s linear'
                                        }} />
                                    </div>
                                </div>

                                {/* kW digital readout */}
                                <div className="text-center">
                                    <div className="value-display" style={{ fontSize: '2.5rem', fontWeight: 'bold', lineHeight: 1, color: '#FFF' }}>
                                        {fmt(power)}
                                    </div>
                                    <div className="label-small" style={{ fontSize: '0.9rem' }}>kW</div>
                                </div>
                            </Col>
                        </Row>
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
