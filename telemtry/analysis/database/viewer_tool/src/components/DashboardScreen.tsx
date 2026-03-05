'use client';

import React, { useState, useEffect, useRef } from 'react';
import RadialGauge from './dashboard/RadialGauge';
import VerticalGauge from './dashboard/VerticalGauge';
import FixedAspectContainer from './dashboard/FixedAspectContainer';

const BRAND_COLOR = "#BF5700";

const DashboardScreen: React.FC = () => {
    const [speed, setSpeed] = useState(0);
    const [power, setPower] = useState(0);
    const [charge, setCharge] = useState(100);
    const [temp, setTemp] = useState(40);

    const [odometer, setOdometer] = useState(90.5);
    const [lapDelta, setLapDelta] = useState(0);
    const [energyDelta, setEnergyDelta] = useState(0);
    const [lapsRemaining, setLapsRemaining] = useState(20);
    const [alerts, setAlerts] = useState<string[]>([]);
    const [signalStrength, setSignalStrength] = useState(4);
    const [telemetryStatus, setTelemetryStatus] = useState(true);

    const simState = useRef({
        phase: 'accel',
        speed: 0,
        targetSpeed: 60,
        accelRate: 0.5,
        timer: 0
    });

    useEffect(() => {
        const interval = setInterval(() => {
            const { phase, targetSpeed, accelRate } = simState.current;
            let { speed, timer } = simState.current;
            let newPower = 0;

            if (phase === 'accel') {
                if (speed < targetSpeed) {
                    speed += accelRate;
                    newPower = 20 + (accelRate * 50) + (speed/100 * 30);
                } else {
                    simState.current.phase = 'cruise';
                    simState.current.timer = 20 + Math.random() * 50;
                }
            } else if (phase === 'cruise') {
                timer--;
                speed += (Math.random() - 0.5) * 0.1;
                newPower = 10 + (speed/100 * 20) + (Math.random() * 2);

                if (timer <= 0) {
                    if (speed > 50 && Math.random() > 0.3) {
                        simState.current.phase = 'brake';
                        simState.current.targetSpeed = Math.random() * 20;
                        simState.current.accelRate = 0.5 + Math.random() * 1.0;
                    } else {
                        simState.current.phase = 'accel';
                        simState.current.targetSpeed = Math.min(100, speed + 20 + Math.random() * 30);
                        simState.current.accelRate = 0.2 + Math.random() * 0.6;
                    }
                }
            } else if (phase === 'brake') {
                if (speed > targetSpeed) {
                    speed -= accelRate;
                    newPower = -5 - (accelRate * 20);
                } else {
                    simState.current.phase = 'accel';
                    simState.current.targetSpeed = 40 + Math.random() * 40;
                    simState.current.accelRate = 0.3 + Math.random() * 0.5;
                }
            }

            speed = Math.max(0, Math.min(speed, 100));

            simState.current.speed = speed;
            simState.current.timer = timer;

            setSpeed(speed);
            setPower(Math.min(Math.max(newPower, -80), 80));

            setCharge(prev => Math.max(0, prev - (newPower > 0 ? 0.05 : -0.01)));
            setTemp(prev => Math.min(100, Math.max(20, prev + (newPower > 50 ? 0.05 : -0.02))));

            setOdometer(prev => prev + (speed / 3600 / 10));

            setLapDelta(parseFloat((Math.sin(Date.now() / 1000) * 2).toFixed(2)));
            setEnergyDelta(parseFloat((Math.cos(Date.now() / 1000) * 5).toFixed(1)));

            if (Math.random() > 0.95) {
                setSignalStrength(Math.floor(Math.random() * 5));
            }

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

    useEffect(() => {
        const baseConsumption = 4.0;
        const fluctuation = (Math.sin(Date.now() / 2000) * 0.5) + (Math.random() * 0.2);
        const currentConsumption = baseConsumption + fluctuation;
        const val = currentConsumption > 0 ? charge / currentConsumption : 0;
        setLapsRemaining(val);
    }, [charge]);

    const getDeltaColor = (val: number, inverse: boolean = false) => {
        if (val === 0) return "#888";
        if (val < 0) return inverse ? "#FF3333" : "#00FF66";
        return inverse ? "#00FF66" : "#FF3333";
    };

    return (
        <FixedAspectContainer width={800} height={480}>
            <div
                className="relative overflow-hidden text-white"
                style={{
                    width: '800px',
                    height: '480px',
                    background: 'radial-gradient(circle at center, #1a1a1a 0%, #000000 100%)',
                    fontFamily: "'Segoe UI', 'Roboto', sans-serif"
                }}
            >
            {/* Alerts Overlay */}
            {alerts.length > 0 && (
                <div
                    className="absolute left-0 w-full text-center text-black font-extrabold uppercase z-[90]"
                    style={{
                        top: '50px',
                        padding: '10px 12px',
                        fontSize: '1.2rem',
                        letterSpacing: '2px',
                        animation: 'flash-alert 1s infinite',
                        boxShadow: '0 4px 20px rgba(255, 215, 0, 0.5)',
                        borderBottomLeftRadius: '12px',
                        borderBottomRightRadius: '12px',
                        backgroundColor: 'rgba(255, 215, 0, 0.95)'
                    }}
                >
                    {alerts.join(" • ")}
                </div>
            )}

            {/* Top Panel - Lap Delta */}
            <div
                className="absolute top-0 left-0 w-full z-[100] flex items-center justify-center"
                style={{
                    height: '60px',
                    borderRadius: '0 0 12px 12px',
                    background: 'rgba(255, 255, 255, 0.05)',
                    backdropFilter: 'blur(12px)',
                    border: '1px solid rgba(255, 255, 255, 0.1)',
                    borderTop: 'none',
                    boxShadow: '0 4px 20px rgba(0,0,0,0.5)'
                }}
            >
                {/* Left: Connectivity */}
                <div className="absolute left-4 top-1/2 -translate-y-1/2 flex items-center gap-2">
                    <div
                        className="w-2.5 h-2.5 rounded-full"
                        style={{
                            backgroundColor: telemetryStatus ? '#00FF66' : '#FF3333',
                            boxShadow: telemetryStatus ? '0 0 8px #00FF66' : '0 0 8px #FF3333'
                        }}
                    />
                    <span className="text-xs uppercase tracking-wider text-gray-400">5G</span>
                    <div className="flex items-end gap-0.5 h-3.5">
                        {[1, 2, 3, 4].map(bar => (
                            <div
                                key={bar}
                                className="w-1 rounded-sm"
                                style={{
                                    height: `${bar * 25}%`,
                                    backgroundColor: bar <= signalStrength ? '#fff' : 'rgba(255,255,255,0.2)'
                                }}
                            />
                        ))}
                    </div>
                </div>

                {/* Center: Lap Delta */}
                <div className="absolute left-1/2 top-1/2 -translate-x-1/2 -translate-y-1/2 h-full flex items-center justify-center" style={{ width: '300px' }}>
                    <span className="absolute left-0 text-xs uppercase tracking-wider text-gray-500">Lap Delta</span>
                    <div className="relative flex items-center">
                        <span
                            className="font-bold"
                            style={{ fontSize: '2rem', color: getDeltaColor(lapDelta) }}
                        >
                            {lapDelta > 0 ? "+" : lapDelta < 0 ? "-" : ""}{Math.floor(Math.abs(lapDelta))}
                        </span>
                        <span
                            className="font-bold"
                            style={{ fontSize: '2rem', color: getDeltaColor(lapDelta) }}
                        >
                            .
                        </span>
                        <span
                            className="font-bold"
                            style={{ fontSize: '2rem', color: getDeltaColor(lapDelta) }}
                        >
                            {Math.abs(lapDelta).toFixed(2).split('.')[1]}
                        </span>
                        <span className="ml-1 text-sm text-gray-500">s</span>
                    </div>
                </div>

                {/* Right: Laps Remaining */}
                <div className="absolute right-4 top-1/2 -translate-y-1/2 text-right">
                    <div className="text-xs uppercase tracking-wider text-gray-500">Laps Rem.</div>
                    <div
                        className="font-bold"
                        style={{
                            fontSize: '1.5rem',
                            fontFamily: "'Consolas', 'Monaco', monospace",
                            textShadow: '0 0 10px rgba(255,255,255,0.2)'
                        }}
                    >
                        {lapsRemaining.toFixed(1)}
                    </div>
                </div>
            </div>

            {/* Main Content - 3 Column Layout */}
            <div className="flex h-full" style={{ paddingTop: '60px', paddingBottom: '80px' }}>
                {/* Left Column: Temp */}
                <div className="flex-1 flex flex-col items-center justify-center">
                    <div className="text-xs uppercase tracking-wider text-gray-500 mb-1">TEMP</div>
                    <VerticalGauge
                        value={temp}
                        min={0}
                        max={100}
                        label=""
                        color={temp > 80 ? "#ff0000" : BRAND_COLOR}
                        height={180}
                        width={35}
                    />
                    <div
                        className="mt-1 font-bold"
                        style={{
                            fontSize: '1.75rem',
                            fontFamily: "'Consolas', 'Monaco', monospace",
                            textShadow: '0 0 10px rgba(255,255,255,0.2)'
                        }}
                    >
                        {Math.round(temp * 9/5 + 32)}
                        <span className="text-sm ml-1 text-gray-500">°F</span>
                    </div>
                </div>

                {/* Center Column: Speed & Power */}
                <div className="flex-[3] flex flex-col items-center justify-center">
                    <div className="relative">
                        <RadialGauge
                            value={speed}
                            min={0}
                            max={100}
                            label=""
                            size={280}
                            color={BRAND_COLOR}
                            numTicks={10}
                            strokeWidth={16}
                            className="glow-orange"
                            showValueText={false}
                        />
                        {/* Center Text Overlay */}
                        <div className="absolute top-[40%] left-1/2 -translate-x-1/2 -translate-y-1/2 text-center">
                            <div
                                className="font-bold leading-none"
                                style={{
                                    fontSize: '4rem',
                                    fontFamily: "'Consolas', 'Monaco', monospace",
                                    textShadow: '0 0 10px rgba(255,255,255,0.2)'
                                }}
                            >
                                {Math.round(speed)}
                            </div>
                            <div className="text-xs uppercase tracking-wider text-gray-500">MPH</div>
                        </div>

                        {/* Horizontal Energy Bar */}
                        <div className="absolute top-[60%] left-1/2 -translate-x-1/2 w-36 text-center">
                            <div className="w-full h-2 rounded relative overflow-hidden" style={{ background: 'rgba(255,255,255,0.1)' }}>
                                {/* Center Marker */}
                                <div className="absolute left-1/2 top-0 bottom-0 w-0.5 -translate-x-1/2 z-10" style={{ background: 'rgba(255,255,255,0.3)' }} />
                                {/* Fill */}
                                <div
                                    className="absolute top-0 bottom-0"
                                    style={{
                                        left: power < 0 ? `${50 - (Math.min(Math.abs(power), 80)/80)*50}%` : '50%',
                                        width: `${(Math.min(Math.abs(power), 80)/80)*50}%`,
                                        background: power < 0 ? 'linear-gradient(to right, #00CC00, #00FF66)' : 'linear-gradient(to left, #FF0000, #BF5700)',
                                        transition: 'all 0.1s linear'
                                    }}
                                />
                            </div>
                        </div>

                        {/* kW Digital Readout */}
                        <div className="absolute top-[75%] left-1/2 -translate-x-1/2 -translate-y-1/2 text-center">
                            <div
                                className="font-bold leading-none"
                                style={{
                                    fontSize: '2rem',
                                    fontFamily: "'Consolas', 'Monaco', monospace",
                                    textShadow: '0 0 10px rgba(255,255,255,0.2)'
                                }}
                            >
                                {Math.round(power)}
                            </div>
                            <div className="text-xs uppercase tracking-wider text-gray-500">kW</div>
                        </div>
                    </div>
                </div>

                {/* Right Column: SOC */}
                <div className="flex-1 flex flex-col items-center justify-center">
                    <div className="text-xs uppercase tracking-wider text-gray-500 mb-1">SOC</div>
                    <VerticalGauge
                        value={charge}
                        min={0}
                        max={100}
                        label=""
                        color="#FFD700"
                        height={180}
                        width={35}
                    />
                    <div
                        className="mt-1 font-bold"
                        style={{
                            fontSize: '1.75rem',
                            fontFamily: "'Consolas', 'Monaco', monospace",
                            textShadow: '0 0 10px rgba(255,255,255,0.2)'
                        }}
                    >
                        {Math.round(charge)}
                        <span className="text-sm ml-1 text-gray-500">%</span>
                    </div>
                </div>
            </div>

            {/* Bottom Panel - Energy Delta & Odometer */}
            <div
                className="absolute bottom-0 left-0 w-full z-[100]"
                style={{
                    height: '80px',
                    borderRadius: '12px 12px 0 0',
                    background: 'rgba(255, 255, 255, 0.05)',
                    backdropFilter: 'blur(12px)',
                    border: '1px solid rgba(255, 255, 255, 0.1)',
                    boxShadow: '0 4px 20px rgba(0,0,0,0.5)'
                }}
            >
                {/* Left: System Status */}
                <div className="absolute left-0 top-1/2 -translate-y-1/2 text-center w-24">
                    <div className="text-xs uppercase tracking-wider text-gray-500">System</div>
                    <div
                        className="font-bold"
                        style={{
                            fontSize: '1.5rem',
                            color: '#00FF66',
                            textShadow: '0 0 5px rgba(0,255,100,0.5)'
                        }}
                    >
                        OK
                    </div>
                </div>

                {/* Vertical Divider */}
                <div
                    className="absolute top-1/2 -translate-y-1/2"
                    style={{
                        left: '100px',
                        height: '60%',
                        width: '1px',
                        background: 'rgba(255, 255, 255, 0.1)'
                    }}
                />

                {/* Energy Delta (Center) */}
                <div className="absolute left-1/2 top-1/2 -translate-x-1/2 -translate-y-1/2 flex items-center justify-center" style={{ width: '300px', height: '60px' }}>
                    <span className="absolute left-0 text-xs uppercase tracking-wider text-gray-500">Energy Delta</span>
                    <div className="relative flex items-center">
                        <span
                            className="font-bold"
                            style={{ fontSize: '2rem', color: getDeltaColor(energyDelta, true) }}
                        >
                            {energyDelta > 0 ? "+" : energyDelta < 0 ? "-" : ""}{Math.floor(Math.abs(energyDelta))}
                        </span>
                        <span
                            className="font-bold"
                            style={{ fontSize: '2rem', color: getDeltaColor(energyDelta, true) }}
                        >
                            .
                        </span>
                        <span
                            className="font-bold"
                            style={{ fontSize: '2rem', color: getDeltaColor(energyDelta, true) }}
                        >
                            {Math.abs(energyDelta).toFixed(1).split('.')[1]}
                        </span>
                        <span className="ml-1 text-sm text-gray-500">Wh</span>
                    </div>
                </div>

                {/* Odometer (Right) */}
                <div className="absolute right-4 top-1/2 -translate-y-1/2 flex flex-col items-center">
                    <div className="text-xs uppercase tracking-wider text-gray-500">Odometer</div>
                    <div
                        className="font-bold"
                        style={{
                            fontSize: '1.1rem',
                            fontFamily: "'Consolas', 'Monaco', monospace"
                        }}
                    >
                        {odometer.toFixed(1)} <span className="text-gray-400 text-sm">miles</span>
                    </div>
                </div>
            </div>

                {/* CSS Animation for alerts */}
                <style jsx>{`
                    @keyframes flash-alert {
                        0%, 100% { background-color: rgba(255, 215, 0, 0.95); }
                        50% { background-color: rgba(255, 215, 0, 0.5); }
                    }
                `}</style>
            </div>
        </FixedAspectContainer>
    );
};

export default DashboardScreen;
