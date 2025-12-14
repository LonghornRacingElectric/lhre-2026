import React, { useState, useEffect } from 'react';
import './ScreenTwo.css';

// Screen Two: Shutdown Circuit Status
// Resolution: 800 x 480
// Theme: Modern EV (Dark, Glassmorphism, Glowing)

interface ShutdownItem {
    id: number;
    name: string;
    description?: string; // Optional longer description if needed
}

const ITEMS: ShutdownItem[] = [
    { id: 1, name: "BSPD", description: "Brake System Plausibility Device" },
    { id: 2, name: "BMS", description: "Battery Management System" },
    { id: 3, name: "IMD", description: "Insulation Monitoring Device" },
    { id: 4, name: "BOTS", description: "Brake Over-Travel Switch" },
    { id: 5, name: "L-ESTOP", description: "Left E-Stop Button" },
    { id: 6, name: "BATTERY HVIL", description: "High Voltage Interlock Loop" },
    { id: 7, name: "CRASH SENSOR", description: "" },
    { id: 8, name: "F-ESTOP", description: "Front E-Stop Button" },
    { id: 9, name: "R-ESTOP", description: "Right E-Stop Button" },
    { id: 10, name: "TSMS", description: "Tractive System Master Switch" },
    { id: 11, name: "MSD HVIL", description: "Manual Service Disconnect HVIL" },
    { id: 12, name: "E-METER HVIL", description: "Energy Meter HVIL" },
];

const ScreenTwo: React.FC = () => {
    // -------------------------------------------------------------------------
    // DATA HOOKS & SIMULATION
    // -------------------------------------------------------------------------
    // In a real app, this would be an array of booleans from telemetry
    // index 0 -> item 1, etc.
    const [statuses, setStatuses] = useState<boolean[]>(Array(12).fill(true));

    useEffect(() => {
        // Simulation: Randomly trip a sensor every few seconds to show UI state
        const interval = setInterval(() => {
            setStatuses(prev => {
                const newStatuses = [...prev];
                // 10% chance to flip a random bit to false (bad), 90% chance to reset to true
                if (Math.random() > 0.8) {
                    const idx = Math.floor(Math.random() * 12);
                    newStatuses[idx] = !newStatuses[idx]; 
                } else {
                     // Heal randomly
                     const idx = Math.floor(Math.random() * 12);
                     newStatuses[idx] = true;
                }
                return newStatuses;
            });
        }, 2000);
        return () => clearInterval(interval);
    }, []);

    // -------------------------------------------------------------------------

    return (
        <div className="shutdown-container">
            <div className="shutdown-grid">
                {ITEMS.map((item, index) => {
                    const isGood = statuses[index];
                    const statusClass = isGood ? 'status-good' : 'status-bad';
                    
                    return (
                        <div key={item.id} className={`shutdown-item ${statusClass}`}>
                            <div className="item-number">{item.id}</div>
                            <div className="d-flex flex-column justify-content-center" style={{ flexGrow: 1 }}>
                                <div className="item-name">{item.name}</div>
                                {item.description && (
                                    <div style={{ fontSize: '0.7rem', color: '#aaa', marginTop: '-2px' }}>
                                        {item.description}
                                    </div>
                                )}
                            </div>
                            <div className="status-indicator"></div>
                            {/* Optional Text: <div className="status-text">{isGood ? "OK" : "FAULT"}</div> */}
                        </div>
                    );
                })}
            </div>
        </div>
    );
};

export default ScreenTwo;