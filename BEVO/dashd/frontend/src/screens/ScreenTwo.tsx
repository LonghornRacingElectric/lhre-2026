import React from 'react';
import { useDash } from '../context/DashContext';
import { SHUTDOWN_NAMES } from '../types/DashData';
import TopTray from '../components/TopTray';
import './ScreenTwo.css';

// Screen Two: Shutdown Circuit Status
// Resolution: 800 x 480
// Theme: Modern EV (Dark, Glassmorphism, Glowing)
//
// Cells fall into two buckets:
//   1. Items dashd emits in CanData.shutdown[] — driven by SHUTDOWN_NAMES.
//      shutdownIndex points at the matching position in that array.
//   2. Placeholder items the electrical team hasn't put on CAN yet
//      (shutdownIndex = null). These render permanently as "status-unknown"
//      with an "AWAITING FIRMWARE" subtitle.

interface ShutdownCell {
    name: string;
    description: string;
    shutdownIndex: number | null;
}

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

const PLACEHOLDER_LABEL = "Awaiting firmware";

const CELLS: ShutdownCell[] = [
    ...SHUTDOWN_NAMES.map((name, index) => ({
        name,
        description: DESCRIPTIONS[name] ?? "",
        shutdownIndex: index,
    })),
    { name: "TSMS",     description: "Tractive Systems Master Switch", shutdownIndex: null },
    { name: "MSD HVIL", description: "Manual Service Disconnect",      shutdownIndex: null },
    { name: "BOTS",     description: "Brake Over-Travel Switch",       shutdownIndex: null },
    { name: "L-ESTOP",  description: "Left E-Stop",                    shutdownIndex: null },
    { name: "R-ESTOP",  description: "Right E-Stop",                   shutdownIndex: null },
    { name: "D-ESTOP",  description: "Dash E-Stop",                    shutdownIndex: null },
];

const ScreenTwo: React.FC = () => {
    const { data } = useDash();
    const shutdown = data?.can.shutdown;

    return (
        <div className="shutdown-container">
            <TopTray screenLabel="Shutdown" />
            <div className="shutdown-grid">
                {CELLS.map((cell, gridIndex) => {
                    const status = cell.shutdownIndex !== null && shutdown
                        ? (shutdown[cell.shutdownIndex] ?? null)
                        : null;
                    const statusClass = status === null
                        ? 'status-unknown'
                        : status ? 'status-good' : 'status-bad';
                    const subtitle = cell.shutdownIndex === null
                        ? PLACEHOLDER_LABEL
                        : cell.description;

                    return (
                        <div key={cell.name} className={`shutdown-item ${statusClass}`}>
                            <div className="item-number">{gridIndex + 1}</div>
                            <div className="d-flex flex-column justify-content-center" style={{ flexGrow: 1 }}>
                                <div className="item-name">{cell.name}</div>
                                {subtitle && (
                                    <div className="item-description">
                                        {subtitle}
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
