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
    const [odometer, setOdometer] = useState(90.5);
    const [lapDelta, setLapDelta] = useState(0); 
    const [energyDelta, setEnergyDelta] = useState(0); 
    const [lapsRemaining, setLapsRemaining] = useState(20);
    const [alerts, setAlerts] = useState<string[]>([]);
    const [signalStrength, setSignalStrength] = useState(4); // 0-4 bars
    const [telemetryStatus, setTelemetryStatus] = useState(true); // true: broadcasting, false: not
    
    // Physics Simulation State
    const simState = React.useRef({ 
        phase: 'accel', 
        speed: 0, 
        targetSpeed: 60, 
        accelRate: 0.5, 
        timer: 0 
    });

    // Simulation Effect
    useEffect(() => {
        const interval = setInterval(() => {
            let { phase, speed, targetSpeed, accelRate, timer } = simState.current;
            let newPower = 0;

            if (phase === 'accel') {
                if (speed < targetSpeed) {
                    speed += accelRate;
                    // Power high during accel, proportional to rate + drag
                    newPower = 20 + (accelRate * 50) + (speed/100 * 30);
                } else {
                    // Reached target, switch to cruise
                    simState.current.phase = 'cruise';
                    simState.current.timer = 20 + Math.random() * 50; // Cruise for 2-7 seconds
                }
            } else if (phase === 'cruise') {
                timer--;
                // Slight speed fluctuation
                speed += (Math.random() - 0.5) * 0.1;
                // Power just overcomes drag
                newPower = 10 + (speed/100 * 20) + (Math.random() * 2);
                
                if (timer <= 0) {
                    // Decision: Brake or Accel?
                    if (speed > 50 && Math.random() > 0.3) {
                        simState.current.phase = 'brake';
                        simState.current.targetSpeed = Math.random() * 20; // Slow down to 0-20
                        simState.current.accelRate = 0.5 + Math.random() * 1.0; // Brake intensity
                    } else {
                        simState.current.phase = 'accel';
                        simState.current.targetSpeed = Math.min(100, speed + 20 + Math.random() * 30);
                        simState.current.accelRate = 0.2 + Math.random() * 0.6;
                    }
                }
            } else if (phase === 'brake') {
                if (speed > targetSpeed) {
                    speed -= accelRate;
                    // Regen power
                    newPower = -5 - (accelRate * 20); 
                } else {
                    // Reached target (low speed), switch to accel
                    simState.current.phase = 'accel';
                    simState.current.targetSpeed = 40 + Math.random() * 40; // New target
                    simState.current.accelRate = 0.3 + Math.random() * 0.5;
                }
            }

            // Clamp speed
            speed = Math.max(0, Math.min(speed, 100));
            
            // Update ref
            simState.current.speed = speed;
            simState.current.timer = timer;

            setSpeed(speed);
            setPower(Math.min(Math.max(newPower, -80), 80)); // Clamp -80 to 80
            
            setCharge(prev => Math.max(0, prev - (newPower > 0 ? 0.05 : -0.01))); 
            setTemp(prev => Math.min(100, Math.max(20, prev + (newPower > 50 ? 0.05 : -0.02)))); 
            
            setOdometer(prev => prev + (speed / 3600 / 10)); 
            
            setLapDelta(prev => parseFloat((Math.sin(Date.now() / 1000) * 2).toFixed(2))); 
            setEnergyDelta(prev => parseFloat((Math.cos(Date.now() / 1000) * 5).toFixed(1))); 

            // TODO: Hook up to real 5G module signal strength
            if (Math.random() > 0.95) {
                setSignalStrength(Math.floor(Math.random() * 5));
            }

            // TODO: Hook up to real telemetry system status
            if (Math.random() > 0.98) {
                setTelemetryStatus(prev => !prev);
            }

            if (Math.random() > 0.995) { 
                setAlerts(["High Battery Temp"]);
            } else if (Math.random() > 0.98) {
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
                left: '0', 
                zIndex: 100, 
                height: '60px', /* Reduced height */
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

                {/* Center: Lap Delta */}
                <div style={{ 
                    position: 'absolute', 
                    left: '50%', 
                    top: '50%', 
                    transform: 'translate(-50%, -50%)', 
                    zIndex: 100, 
                    height: '100%',
                    width: '500px', // Fixed width similar to energy delta
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

                {/* Right: Laps Remaining */}
                <div style={{ position: 'absolute', right: '30px', top: '50%', transform: 'translateY(-50%)', textAlign: 'right' }}>
                    <div className="label-small" style={{ marginBottom: 0 }}>Laps Rem.</div>
                    <div className="value-display" style={{ fontSize: '1.5rem', fontWeight: 'bold', lineHeight: 1 }}>{lapsRemaining.toFixed(1)}</div>
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
                                    value={temp} 
                                    min={0} 
                                    max={100} 
                                    label=""
                                    color={temp > 80 ? "#ff0000" : BRAND_COLOR} 
                                    height={220} /* Increased height */
                                    width={40}
                                />
                                <div className="text-center value-display" style={{ fontSize: '2rem', fontWeight: 'bold' }}>{Math.round(temp * 9/5 + 32)}<span style={{ fontSize: '1rem', marginLeft: '5px', color: '#888' }}>°F</span></div>
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
                                        value={speed} 
                                        min={0} 
                                        max={100} 
                                        label="" 
                                        size={340} 
                                        color={BRAND_COLOR}
                                        numTicks={10}
                                        strokeWidth={18}
                                        className="glow-orange"
                                        showValueText={false} // Hide internal value text
                                    />
                                    {/* Center Text Overlay */}
                                    <div style={{ position: 'absolute', top: '40%', left: '50%', transform: 'translate(-50%, -50%)', textAlign: 'center' }}>
                                        <div className="value-display" style={{ fontSize: '5rem', fontWeight: 'bold', lineHeight: 1 }}>{Math.round(speed)}</div>
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
                                                left: power < 0 ? `${50 - (Math.min(Math.abs(power), 80)/80)*50}%` : '50%',
                                                width: `${(Math.min(Math.abs(power), 80)/80)*50}%`,
                                                background: power < 0 ? 'linear-gradient(to right, #00CC00, #00FF66)' : 'linear-gradient(to left, #FF0000, #BF5700)',
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
                                            {Math.round(power)}
                                        </div>
                                        <div className="label-small" style={{ fontSize: '0.9rem' }}>kW</div>
                                    </div>
                                </div>
                                
                            </Col>

                            {/* Power Section (Temporarily removed) 
                            <Col xs={6} className="d-flex flex-column align-items-center justify-content-center">
                                <div style={{ position: 'relative' }}>
                                    <RadialGauge 
                                        value={power} 
                                        min={-80} 
                                        max={80} 
                                        label="" 
                                        size={240} 
                                        mode="bidirectional"
                                        numTicks={8}
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
                            */}
                        </Row>
                    </Col>

                    {/* 4. Right Column: Charge & Laps */}
                    <Col xs={2} className="h-100-flex align-items-center" style={{ paddingTop: '70px', paddingBottom: '90px' }}>
                        <div className="dash-card d-flex flex-column align-items-center" style={{ width: '100%', height: '100%', justifyContent: 'center', border: 'none' }}>
                            <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '2px' }}>
                                <div className="label-small text-center" style={{ fontSize: '1rem', letterSpacing: '2px' }}>SOC</div>
                                <VerticalGauge 
                                    value={charge} 
                                    min={0} 
                                    max={100} 
                                    label=""
                                    color="#FFD700" // Yellow
                                    height={220} /* Slightly taller */
                                    width={40}   /* Wider */
                                />
                                <div className="text-center value-display" style={{ fontSize: '2rem', fontWeight: 'bold' }}>{Math.round(charge)}<span style={{ fontSize: '1rem', marginLeft: '5px', color: '#888' }}>%</span></div>
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
                height: '80px', /* Reduced height for single row */
                width: '100%',
                borderRadius: '12px 12px 0 0', /* Rounded top corners, square bottom */
                background: 'rgba(255, 255, 255, 0.05)',
                backdropFilter: 'blur(12px)',
                border: '1px solid rgba(255, 255, 255, 0.1)',
                boxShadow: '0 4px 20px rgba(0,0,0,0.5)',
                padding: 0
            }}>
                {/* Left: System Status */}
                <div style={{ position: 'absolute', left: '0', top: '50%', transform: 'translateY(-50%)', textAlign: 'center', width: '120px' }}>
                    <div className="label-small" style={{ marginBottom: 0 }}>System</div>
                    <div className="value-display" style={{ fontSize: '1.5rem', fontWeight: 'bold', color: '#00FF66', textShadow: '0 0 5px rgba(0,255,100,0.5)', lineHeight: 1 }}>OK</div>
                </div>

                {/* Vertical Divider for System Status */}
                <div style={{
                    position: 'absolute',
                    left: '120px', /* Position to the right of System Status */
                    top: '50%',
                    transform: 'translateY(-50%)',
                    height: '60%',
                    width: '1px',
                    background: 'rgba(255, 255, 255, 0.1)' /* Faint white */
                }} />

                {/* Energy Delta (Center) */}
                <div style={{ 
                    position: 'absolute', 
                    left: '50%', 
                    top: '50%', 
                    transform: 'translate(-50%, -50%)', 
                    zIndex: 100, 
                    height: '60px',
                    width: '500px' // Increased width to prevent title overlap
                    // No 'position: relative' here as it's already absolute
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
                {/* Odometer (Right) */}
                <div style={{ position: 'absolute', right: '30px', top: '50%', transform: 'translateY(-50%)', display: 'flex', flexDirection: 'column', alignItems: 'center' }}>
                    <div className="label-small" style={{ fontSize: '0.75rem', marginBottom: 0 }}>Odometer</div>
                    <div className="value-display" style={{ fontSize: '1.2rem', fontWeight: 'bold' }}>{odometer.toFixed(1)} <span className="unit-label">miles</span></div>
                </div>
            </div>
        </div>
    );
};

export default ScreenOne;
