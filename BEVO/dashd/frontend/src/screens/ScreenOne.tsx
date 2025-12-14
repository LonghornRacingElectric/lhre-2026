import React, { useState, useEffect } from 'react';
import { Container, Row, Col } from 'react-bootstrap';
import RadialGauge from '../components/RadialGauge';
import VerticalGauge from '../components/VerticalGauge';
import './ScreenOne.css';

// Screen One: Main Dashboard (Modern EV Style)
// Resolution: 800 x 480

const ScreenOne: React.FC = () => {
    // -------------------------------------------------------------------------
    // DATA HOOKS & SIMULATION
    // -------------------------------------------------------------------------
    
    // Existing Metrics
    const [speed, setSpeed] = useState(0);
    const [power, setPower] = useState(0);
    const [charge, setCharge] = useState(100);
    const [temp, setTemp] = useState(40);

    // New Metrics
    const [odometer, setOdometer] = useState(1234.5);
    const [lapDelta, setLapDelta] = useState(0); 
    const [energyDelta, setEnergyDelta] = useState(0); 
    const [lapsRemaining, setLapsRemaining] = useState(20);
    const [alerts, setAlerts] = useState<string[]>([]);
    const [signalStrength, setSignalStrength] = useState(4); // 0-4 bars
    const [telemetryStatus, setTelemetryStatus] = useState(true); // true: broadcasting, false: not

    // Simulation Effect
    useEffect(() => {
        const interval = setInterval(() => {
            setSpeed(prev => (prev + 1) % 120); 
            setPower(prev => {
                const next = prev + 5;
                return next > 100 ? -50 : next; 
            });
            setCharge(prev => Math.max(0, prev - 0.05)); 
            setTemp(prev => Math.min(100, prev + 0.1));
            setOdometer(prev => prev + 0.01);
            setLapDelta(prev => parseFloat((Math.sin(Date.now() / 1000) * 2).toFixed(2))); 
            setEnergyDelta(prev => parseFloat((Math.cos(Date.now() / 1000) * 5).toFixed(1))); 

            // TODO: Hook up to real 5G module signal strength
            // Simulation: Randomly change signal strength occasionally
            if (Math.random() > 0.95) {
                setSignalStrength(Math.floor(Math.random() * 5));
            }

            // TODO: Hook up to real telemetry system status
            // Simulation: Randomly change telemetry status occasionally
            if (Math.random() > 0.98) {
                setTelemetryStatus(prev => !prev);
            }

            if (Math.random() > 0.99) {
                setAlerts(["High Battery Temp"]);
            } else if (Math.random() > 0.95) {
                 setAlerts([]);
            }

        }, 100);
        return () => clearInterval(interval);
    }, []);

    // Laps Remaining Calculation
    useEffect(() => {
        // Toy implementation: varying energy consumption per lap
        // Base consumption: ~4% per lap. Fluctuation: +/- 0.5%
        const baseConsumption = 4.0;
        const fluctuation = (Math.sin(Date.now() / 2000) * 0.5) + (Math.random() * 0.2); 
        const currentConsumption = baseConsumption + fluctuation;
        
        // Calculate laps remaining based on current charge and consumption
        // Prevent divide by zero
        const val = currentConsumption > 0 ? charge / currentConsumption : 0;
        setLapsRemaining(val);
    }, [charge]);

    // -------------------------------------------------------------------------
    // RENDER HELPERS
    // -------------------------------------------------------------------------

    const getDeltaColor = (val: number, inverse: boolean = false) => {
        if (val === 0) return "#888"; // Neutral gray
        if (val < 0) return inverse ? "#FF3333" : "#00FF66"; // Green for good (negative time)
        return inverse ? "#00FF66" : "#FF3333"; // Red for bad
    };

    const BRAND_COLOR = "#BF5700"; // Burnt Orange

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
                left: '50%', 
                transform: 'translateX(-50%)', 
                zIndex: 100, 
                height: '60px', /* Reduced height */
                width: '66.66%',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                borderRadius: '0 0 12px 12px',
                borderTop: 'none'
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

                    {/* Integer Part - Right of Center */}
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

                    {/* Fraction Part - Left of Center */}
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
            </div>

            <Container fluid style={{ height: '100%' }}>
                <Row style={{ height: '100%' }}>
                    
                    {/* 1. Left Column: Temp */}
                    <Col xs={2} className="h-100-flex align-items-center">
                        <div className="dash-card d-flex flex-column align-items-center" style={{ width: '100%', height: '90%', justifyContent: 'space-around' }}>
                            <div>
                                <div className="label-small text-center">Temp</div>
                                <VerticalGauge 
                                    value={temp} 
                                    min={0} 
                                    max={100} 
                                    label=""
                                    color={temp > 80 ? "#ff0000" : BRAND_COLOR} 
                                    height={220}
                                    width={30}
                                    className={temp > 80 ? "glow-red" : "glow-orange"}
                                />
                                <div className="text-center mt-2 value-display" style={{ fontSize: '1.8rem' }}>{Math.round(temp * 9/5 + 32)}°F</div>
                            </div>
                            
                            <div className="text-center w-100" style={{ borderTop: '1px solid #333', paddingTop: '10px' }}>
                                <div className="label-small">System</div>
                                <div className="value-display" style={{ fontSize: '1.5rem', fontWeight: 'bold', color: '#00FF66', textShadow: '0 0 5px rgba(0,255,100,0.5)' }}>OK</div>
                            </div>
                        </div>
                    </Col>

                    {/* 2. Center Cluster: Speed & Power */}
                    <Col xs={8} className="h-100-flex" style={{ paddingTop: '60px', paddingBottom: '110px' }}>
                        <Row className="h-100">
                            {/* Speed Section */}
                            <Col xs={6} className="d-flex flex-column align-items-center justify-content-center">
                                <div style={{ position: 'relative' }}>
                                    <RadialGauge 
                                        value={speed} 
                                        min={0} 
                                        max={120} 
                                        label="" 
                                        size={240} 
                                        color={BRAND_COLOR}
                                        numTicks={12}
                                        strokeWidth={10}
                                        className="glow-orange"
                                        showValueText={false} // Hide internal value text
                                    />
                                    {/* Center Text Overlay */}
                                    <div style={{ position: 'absolute', top: '50%', left: '50%', transform: 'translate(-50%, -50%)', textAlign: 'center' }}>
                                        <div className="value-display" style={{ fontSize: '4rem', fontWeight: 'bold', lineHeight: 1 }}>{Math.round(speed)}</div>
                                        <div className="label-small" style={{ fontSize: '0.9rem' }}>MPH</div>
                                    </div>
                                </div>
                                
                            </Col>

                            {/* Power Section */}
                            <Col xs={6} className="d-flex flex-column align-items-center justify-content-center">
                                <div style={{ position: 'relative' }}>
                                    <RadialGauge 
                                        value={power} 
                                        min={-50} 
                                        max={100} 
                                        label="" 
                                        size={240} 
                                        color="gradient" 
                                        numTicks={10}
                                        strokeWidth={10}
                                        className="glow-gradient"
                                        showValueText={false} // Hide internal value text
                                    />
                                     <div style={{ position: 'absolute', top: '50%', left: '50%', transform: 'translate(-50%, -50%)', textAlign: 'center' }}>
                                        <div className="value-display" style={{ fontSize: '3rem', fontWeight: 'bold', lineHeight: 1 }}>{Math.round(power)}</div>
                                        <div className="label-small" style={{ fontSize: '0.9rem' }}>kW</div>
                                    </div>
                                </div>
                            </Col>
                        </Row>
                    </Col>

                    {/* 4. Right Column: Charge & Laps */}
                    <Col xs={2} className="h-100-flex align-items-center">
                        <div className="dash-card d-flex flex-column align-items-center" style={{ width: '100%', height: '90%', justifyContent: 'space-around' }}>
                            <div>
                                <div className="label-small text-center">SoC</div>
                                <VerticalGauge 
                                    value={charge} 
                                    min={0} 
                                    max={100} 
                                    label=""
                                    color="#FFD700" // Yellow
                                    height={220}
                                    width={30}
                                    className="glow-yellow"
                                />
                                <div className="text-center mt-2 value-display" style={{ fontSize: '1.8rem' }}>{Math.round(charge)}%</div>
                            </div>
                            
                            <div className="text-center w-100" style={{ borderTop: '1px solid #333', paddingTop: '10px' }}>
                                <div className="label-small">Laps Rem.</div>
                                <div className="value-display" style={{ fontSize: '1.8rem', fontWeight: 'bold' }}>{lapsRemaining.toFixed(1)}</div>
                            </div>
                        </div>
                    </Col>

                </Row>
            </Container>

            {/* Bottom Panel - Energy Delta & Odometer */}
            <div className="dash-card" style={{ 
                position: 'absolute', 
                bottom: '0', 
                left: '50%', 
                transform: 'translateX(-50%)', 
                zIndex: 100, 
                height: '110px', /* Increased height */
                width: '66.66%',
                borderRadius: '12px 12px 0 0',
                borderBottom: 'none',
                padding: 0
            }}>
                {/* Energy Delta (Top Half) */}
                <div style={{ position: 'relative', width: '100%', height: '60px' }}>
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
                    <div style={{ position: 'absolute', width: '100%', height: '100%', top: 0, left: 0 }}>
                        
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

                        {/* Integer Part - Right of Center */}
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

                        {/* Fraction Part - Left of Center */}
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
                </div>

                {/* Odometer (Bottom Half) */}
                <div style={{ position: 'relative', width: '100%', height: '50px', borderTop: '1px solid rgba(255,255,255,0.1)' }}>
                    {/* Left: Odometer */}
                    <div style={{ position: 'absolute', left: '30px', top: '50%', transform: 'translateY(-50%)', display: 'flex', alignItems: 'baseline' }}>
                        <div className="label-small" style={{ fontSize: '0.75rem', marginRight: '10px', marginBottom: 0 }}>Odometer</div>
                        <div className="value-display" style={{ fontSize: '1.2rem', fontWeight: 'bold' }}>{odometer.toFixed(1)} <span className="unit-label">miles</span></div>
                    </div>

                    {/* Right: Signal Strength */}
                    <div style={{ position: 'absolute', right: '30px', top: '50%', transform: 'translateY(-50%)', display: 'flex', alignItems: 'center', gap: '8px' }}>
                        {/* Telemetry Status Light */}
                        {/* TODO: Hook up to real telemetry status */}
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
                                    backgroundColor: bar <= signalStrength ? '#fff' : 'rgba(255,255,255,0.2)',
                                    borderRadius: '1px'
                                }} />
                            ))}
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
};

export default ScreenOne;
