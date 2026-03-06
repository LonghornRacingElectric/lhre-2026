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

const ScreenTwo: React.FC = () => {
    // -------------------------------------------------------------------------
    // DATA HOOKS & SIMULATION
    // -------------------------------------------------------------------------
    // In a real app, this would be an array of booleans from telemetry
    // index 0 -> item 1, etc.
    const [statuses, setStatuses] = useState<boolean[]>(Array(16).fill(true));

    useEffect(() => {
        // Simulation: Randomly trip a sensor every few seconds to show UI state
        const interval = setInterval(() => {
            setStatuses(prev => {
                const newStatuses = [...prev];
                // 10% chance to flip a random bit to false (bad), 90% chance to reset to true
                if (Math.random() > 0.8) {
                    const idx = Math.floor(Math.random() * 16);
                    newStatuses[idx] = !newStatuses[idx]; 
                } else {
                     // Heal randomly
                     const idx = Math.floor(Math.random() * 16);
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
                                    <div className="item-description">
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