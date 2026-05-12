import React from 'react';
import { useDash } from '../context/DashContext';
import { SHUTDOWN_NAMES } from '../types/DashData';
import TopTray from '../components/TopTray';
import './ScreenTwo.css';

// Screen Two: Shutdown Circuit Status
// Resolution: 800 x 480
// Theme: Modern EV (Dark, Glassmorphism, Glowing)
//
// The layout is driven by SHUTDOWN_NAMES (defined in types/DashData) which is
// the authoritative ordering for dashd's shutdown[] array. Descriptions below
// are optional context only — adding/removing entries in SHUTDOWN_NAMES
// automatically updates this screen.

const DESCRIPTIONS: Record<string, string> = {
    "LEG 1": "Shutdown circuit leg 1",
    "LEG 2": "Shutdown circuit leg 2",
    "LEG 3": "Shutdown circuit leg 3",
    "LEG 4": "Shutdown circuit leg 4",
    "BMS": "Battery Management System",
    "IMD": "Insulation Monitoring Device",
    "BSPD": "Brake System Plausibility Device",
    "E-METER": "Energy Meter HVIL",
    "DUI TEMP 1": "DUI thermal shutdown 1",
    "DUI TEMP 2": "DUI thermal shutdown 2",
};

const ScreenTwo: React.FC = () => {
    const { data } = useDash();
    const shutdown = data?.can.shutdown;

    return (
        <div className="shutdown-container">
            <TopTray screenLabel="Shutdown" />
            <div className="shutdown-grid">
                {SHUTDOWN_NAMES.map((name, index) => {
                    // Defensive: if shutdown[] is shorter than expected, treat
                    // the missing slot as "unknown" rather than letting falsy
                    // `undefined` masquerade as a fault.
                    const status = shutdown ? (shutdown[index] ?? null) : null;
                    const statusClass = status === null ? 'status-unknown' : status ? 'status-good' : 'status-bad';
                    const description = DESCRIPTIONS[name];

                    return (
                        <div key={name} className={`shutdown-item ${statusClass}`}>
                            <div className="item-number">{index + 1}</div>
                            <div className="d-flex flex-column justify-content-center" style={{ flexGrow: 1 }}>
                                <div className="item-name">{name}</div>
                                {description && (
                                    <div className="item-description">
                                        {description}
                                    </div>
                                )}
                            </div>
                            <div className="status-indicator"></div>
                        </div>
                    );
                })}
            </div>
        </div>
    );
};

export default ScreenTwo;
