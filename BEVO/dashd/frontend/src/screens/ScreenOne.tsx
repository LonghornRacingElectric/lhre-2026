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
            setLapsRemaining(prev => Math.max(0, parseFloat((prev - 0.001).toFixed(2))));

            if (Math.random() > 0.99) {
                setAlerts(["High Battery Temp"]);
            } else if (Math.random() > 0.95) {
                 setAlerts([]);
            }

        }, 100);
        return () => clearInterval(interval);
    }, []);

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
                                <div className="text-center mt-2 value-display" style={{ fontSize: '1.2rem' }}>{Math.round(temp)}°C</div>
                            </div>
                            
                            <div className="text-center w-100" style={{ borderTop: '1px solid #333', paddingTop: '10px' }}>
                                <div className="label-small">System</div>
                                <div className="value-display" style={{ fontSize: '1.5rem', fontWeight: 'bold', color: '#00FF66', textShadow: '0 0 5px rgba(0,255,100,0.5)' }}>OK</div>
                            </div>
                        </div>
                    </Col>

                    {/* 2. Center Cluster: Speed & Power */}
                    <Col xs={8} className="h-100-flex">
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
                                
                                <div className="dash-card mt-2 d-flex justify-content-between align-items-center px-4" style={{ width: '90%' }}>
                                    <div>
                                        <div className="label-small">Odometer</div>
                                        <div className="value-display" style={{ fontSize: '1.2rem' }}>{odometer.toFixed(1)} <span className="unit-label">km</span></div>
                                    </div>
                                    <div style={{ textAlign: 'right' }}>
                                        <div className="label-small">Lap Delta</div>
                                        <div className="value-display" style={{ fontSize: '1.2rem', color: getDeltaColor(lapDelta) }}>
                                            {lapDelta > 0 ? "+" : ""}{lapDelta} s
                                        </div>
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

                                <div className="dash-card mt-2 d-flex justify-content-center align-items-center px-4" style={{ width: '90%' }}>
                                    <div className="text-center">
                                        <div className="label-small">Energy Delta</div>
                                        <div className="value-display" style={{ fontSize: '1.5rem', color: getDeltaColor(energyDelta, true) }}>
                                            {energyDelta > 0 ? "+" : ""}{energyDelta} <span className="unit-label">Wh</span>
                                        </div>
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
                                <div className="text-center mt-2 value-display" style={{ fontSize: '1.2rem' }}>{Math.round(charge)}%</div>
                            </div>
                            
                            <div className="text-center w-100" style={{ borderTop: '1px solid #333', paddingTop: '10px' }}>
                                <div className="label-small">Laps Rem.</div>
                                <div className="value-display" style={{ fontSize: '1.8rem', fontWeight: 'bold' }}>{lapsRemaining.toFixed(1)}</div>
                            </div>
                        </div>
                    </Col>

                </Row>
            </Container>
        </div>
    );
};

export default ScreenOne;
