'use client';

import React, { useState, useEffect } from 'react';
import FixedAspectContainer from './dashboard/FixedAspectContainer';

interface ShutdownItem {
    id: number;
    name: string;
    description?: string;
}

const ITEMS: ShutdownItem[] = [
    { id: 1, name: "LV Master Switch", description: "" },
    { id: 2, name: "Shutdown Fuse", description: "5A" },
    { id: 3, name: "R-ESTOP", description: "Right E-Stop Button" },
    { id: 4, name: "BMS", description: "Battery Management System Relay" },
    { id: 5, name: "IMD", description: "Insulation Monitoring Device Relay" },
    { id: 6, name: "Battery ACU HVIL", description: "" },
    { id: 7, name: "L-ESTOP", description: "Left E-Stop Button" },
    { id: 8, name: "D-ESTOP", description: "Dash E-Stop Button" },
    { id: 9, name: "Inertial Switch", description: "" },
    { id: 10, name: "BOTS", description: "Brake Over-Travel Switch" },
    { id: 11, name: "BSPD", description: "Brake Systems Plausibility Device Relay" },
    { id: 12, name: "E-Meter HVIL", description: "" },
    { id: 13, name: "MSD HVIL", description: "Manual Service Disconnect" },
    { id: 14, name: "Battery HVIL", description: "" },
    { id: 15, name: "Inverter HVIL", description: "" },
    { id: 16, name: "TSMS", description: "Tractive Systems Master Switch" },
];

const ShutdownScreen: React.FC = () => {
    const [statuses, setStatuses] = useState<boolean[]>(Array(16).fill(true));

    useEffect(() => {
        const interval = setInterval(() => {
            setStatuses(prev => {
                const newStatuses = [...prev];
                if (Math.random() > 0.8) {
                    const idx = Math.floor(Math.random() * 16);
                    newStatuses[idx] = !newStatuses[idx];
                } else {
                    const idx = Math.floor(Math.random() * 16);
                    newStatuses[idx] = true;
                }
                return newStatuses;
            });
        }, 2000);
        return () => clearInterval(interval);
    }, []);

    return (
        <FixedAspectContainer width={800} height={480}>
            <div
                className="p-5 box-border text-white overflow-hidden"
                style={{
                    width: '800px',
                    height: '480px',
                    background: 'radial-gradient(circle at center, #1a1a1a 0%, #000000 100%)',
                    fontFamily: "'Segoe UI', 'Roboto', sans-serif"
                }}
            >
                <div
                    className="grid h-full w-full gap-4"
                    style={{
                        gridTemplateColumns: 'repeat(4, 1fr)',
                        gridTemplateRows: 'repeat(4, 1fr)'
                    }}
                >
                {ITEMS.map((item, index) => {
                    const isGood = statuses[index];

                    return (
                        <div
                            key={item.id}
                            className="relative flex items-center rounded-lg transition-all duration-300"
                            style={{
                                background: isGood
                                    ? 'rgba(255, 255, 255, 0.05)'
                                    : 'rgba(255, 0, 0, 0.15)',
                                backdropFilter: 'blur(5px)',
                                border: isGood
                                    ? '1px solid rgba(0, 255, 100, 0.3)'
                                    : '1px solid rgba(255, 50, 50, 0.5)',
                                boxShadow: isGood
                                    ? '0 0 15px rgba(0, 255, 100, 0.1) inset'
                                    : '0 0 20px rgba(255, 0, 0, 0.3) inset',
                                padding: '0 30px',
                                overflow: 'hidden',
                                animation: isGood ? 'none' : 'pulse-red 1.5s infinite'
                            }}
                        >
                            {/* Item Number */}
                            <div
                                className="absolute left-2 top-1/2 -translate-y-1/2 font-bold opacity-50"
                                style={{
                                    fontFamily: "'Consolas', 'Monaco', monospace",
                                    fontSize: '1.25rem',
                                    color: '#888',
                                    width: '20px'
                                }}
                            >
                                {item.id}
                            </div>

                            {/* Item Content */}
                            <div className="flex flex-col justify-center flex-grow">
                                <div
                                    className="font-semibold uppercase"
                                    style={{
                                        fontSize: '0.75rem',
                                        letterSpacing: '0.5px'
                                    }}
                                >
                                    {item.name}
                                </div>
                                {item.description && (
                                    <div
                                        style={{
                                            fontSize: '0.55rem',
                                            color: '#aaa',
                                            marginTop: '-1px'
                                        }}
                                    >
                                        {item.description}
                                    </div>
                                )}
                            </div>

                            {/* Status Indicator */}
                            <div
                                className="absolute right-3 top-1/2 -translate-y-1/2 rounded-full flex-shrink-0"
                                style={{
                                    width: '12px',
                                    height: '12px',
                                    backgroundColor: isGood ? '#00FF66' : '#FF3333',
                                    boxShadow: isGood ? '0 0 10px #00FF66' : '0 0 15px #FF3333'
                                }}
                            />
                        </div>
                    );
                })}
                </div>

                {/* CSS Animation for pulse */}
                <style jsx>{`
                    @keyframes pulse-red {
                        0% { box-shadow: 0 0 20px rgba(255, 0, 0, 0.3) inset; }
                        50% { box-shadow: 0 0 40px rgba(255, 0, 0, 0.6) inset; }
                        100% { box-shadow: 0 0 20px rgba(255, 0, 0, 0.3) inset; }
                    }
                `}</style>
            </div>
        </FixedAspectContainer>
    );
};

export default ShutdownScreen;
