import React, { useState, useEffect } from 'react';
import { Container, Row, Col } from 'react-bootstrap';
import RadialGauge from '../components/RadialGauge';
import VerticalGauge from '../components/VerticalGauge';

// Screen One: Main Dashboard
// Resolution: 800 x 480

const ScreenOne: React.FC = () => {
    // -------------------------------------------------------------------------
    // DATA HOOKS & SIMULATION
    // -------------------------------------------------------------------------
    // Connect your real data sources here.
    
    // Existing Metrics
    const [speed, setSpeed] = useState(0);
    const [power, setPower] = useState(0);
    const [charge, setCharge] = useState(100);
    const [temp, setTemp] = useState(40);

    // New Metrics
    const [odometer, setOdometer] = useState(1234.5);
    const [lapDelta, setLapDelta] = useState(0); // Seconds. - is faster (Good), + is slower (Bad)
    const [energyDelta, setEnergyDelta] = useState(0); // Wh or %. - is saving (Good), + is over-consuming (Bad)
    const [lapsRemaining, setLapsRemaining] = useState(20);
    const [alerts, setAlerts] = useState<string[]>([]);

    // Simulation Effect (Remove for Production)
    useEffect(() => {
        const interval = setInterval(() => {
            // Simulate changing values
            setSpeed(prev => (prev + 1) % 120); 
            setPower(prev => {
                const next = prev + 5;
                return next > 100 ? -50 : next; 
            });
            setCharge(prev => Math.max(0, prev - 0.05)); 
            setTemp(prev => Math.min(100, prev + 0.1));

            // Simulate Odometer
            setOdometer(prev => prev + 0.1);

            // Simulate Deltas
            setLapDelta(prev => parseFloat((Math.sin(Date.now() / 1000) * 2).toFixed(2))); // Oscillate between -2 and 2
            setEnergyDelta(prev => parseFloat((Math.cos(Date.now() / 1000) * 5).toFixed(1))); 

            // Simulate Laps Remaining (Should be calculated: Charge Remaining / Avg Draw per Lap)
            setLapsRemaining(prev => Math.max(0, parseFloat((prev - 0.001).toFixed(2))));

            // Simulate Alerts
            if (Math.random() > 0.98) {
                setAlerts(["High Battery Temp!"]);
            } else if (Math.random() > 0.98) {
                 setAlerts([]);
            }

        }, 100);
        return () => clearInterval(interval);
    }, []);

    // -------------------------------------------------------------------------
    // RENDER HELPERS
    // -------------------------------------------------------------------------

    const getDeltaColor = (val: number, inverse: boolean = false) => {
        // Default: Negative is Green (Good), Positive is Red (Bad)
        // If inverse: Positive is Green, Negative is Red.
        if (val === 0) return "#FFFFFF";
        if (val < 0) return inverse ? "#FF0000" : "#00FF00";
        return inverse ? "#00FF00" : "#FF0000";
    };

    const BRAND_COLOR = "#BF5700"; // Burnt Orange
    const TEXT_COLOR = "#FFFFFF";
    const BG_COLOR = "#000000";

    return (
        <Container fluid style={{ height: '100vh', backgroundColor: BG_COLOR, color: TEXT_COLOR, overflow: 'hidden', position: 'relative' }}>
            
            {/* Alerts Overlay */}
            {alerts.length > 0 && (
                <div style={{
                    position: 'absolute',
                    top: 0,
                    left: 0,
                    width: '100%',
                    backgroundColor: 'rgba(255, 0, 0, 0.8)',
                    color: 'white',
                    textAlign: 'center',
                    padding: '10px',
                    fontSize: '1.5rem',
                    fontWeight: 'bold',
                    zIndex: 1000,
                    animation: 'flash 1s infinite'
                }}>
                    {alerts.join(", ")}
                </div>
            )}

            <Row style={{ height: '100%' }} className="align-items-center justify-content-center">
                
                {/* 1. Left Column: Charge & Laps Remaining */}
                <Col xs={2} className="d-flex flex-column align-items-center justify-content-center">
                    <VerticalGauge 
                        value={charge} 
                        min={0} 
                        max={100} 
                        label="Charge %" 
                        color={BRAND_COLOR}
                        height={300}
                        width={60}
                    />
                    <div className="mt-4 text-center">
                        <div style={{ fontSize: '0.9rem', color: '#aaa' }}>Laps Rem.</div>
                        <div style={{ fontSize: '1.8rem', fontWeight: 'bold' }}>{lapsRemaining.toFixed(1)}</div>
                    </div>
                </Col>

                {/* 2. Center-Left: Speed & Odometer */}
                <Col xs={4} className="d-flex flex-column align-items-center justify-content-center">
                    <RadialGauge 
                        value={speed} 
                        min={0} 
                        max={120} 
                        label="Speed" 
                        size={280} 
                        color={BRAND_COLOR}
                        numTicks={12}
                    />
                    <div className="mt-2 text-center">
                         <div style={{ fontSize: '0.9rem', color: '#aaa' }}>Odometer</div>
                         <div style={{ fontSize: '1.5rem', fontFamily: 'monospace' }}>{odometer.toFixed(1)} km</div>
                    </div>
                    {/* Lap Delta could also go here or center */}
                    <div className="mt-3 text-center">
                        <div style={{ fontSize: '0.9rem', color: '#aaa' }}>Lap Delta</div>
                        <div style={{ 
                            fontSize: '2rem', 
                            fontWeight: 'bold', 
                            color: getDeltaColor(lapDelta) 
                        }}>
                            {lapDelta > 0 ? "+" : ""}{lapDelta} s
                        </div>
                    </div>
                </Col>

                {/* 3. Center-Right: Power & Energy Delta */}
                <Col xs={4} className="d-flex flex-column align-items-center justify-content-center">
                    <RadialGauge 
                        value={power} 
                        min={-50} 
                        max={100} 
                        label="Power kW" 
                        size={280} 
                        color="gradient" 
                    />
                     {/* Spacer to align with Odometer roughly if needed, or just flow */}
                     <div className="mt-2 text-center">
                         {/* Placeholder or just spacing */}
                         <div style={{ height: '1.5rem', visibility: 'hidden' }}>Spacer</div> 
                         <div style={{ fontSize: '1.5rem', visibility: 'hidden' }}>Spacer</div>
                    </div>

                    <div className="mt-3 text-center">
                        <div style={{ fontSize: '0.9rem', color: '#aaa' }}>Energy Delta</div>
                        <div style={{ 
                            fontSize: '2rem', 
                            fontWeight: 'bold', 
                            color: getDeltaColor(energyDelta) 
                        }}>
                            {energyDelta > 0 ? "+" : ""}{energyDelta} Wh
                        </div>
                    </div>
                </Col>

                {/* 4. Right Column: Temperature */}
                <Col xs={2} className="d-flex flex-column align-items-center justify-content-center">
                    <VerticalGauge 
                        value={temp} 
                        min={0} 
                        max={100} 
                        label="Temp °C" 
                        color={temp > 80 ? "red" : BRAND_COLOR} 
                        height={300}
                        width={60}
                    />
                     <div className="mt-4 text-center">
                        <div style={{ fontSize: '0.9rem', color: '#aaa' }}>Status</div>
                        <div style={{ fontSize: '1.2rem', fontWeight: 'bold', color: 'green' }}>OK</div>
                    </div>
                </Col>

            </Row>
            
            <style>
                {`
                @keyframes flash {
                    0% { opacity: 1; }
                    50% { opacity: 0.5; }
                    100% { opacity: 1; }
                }
                `}
            </style>
        </Container>
    );
};

export default ScreenOne;