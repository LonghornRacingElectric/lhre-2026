import React from 'react';
import ConnectivityIndicator from './ConnectivityIndicator';
import './TopTray.css';

// Standard 60px top tray used by every screen except ScreenOne (which has
// a richer tray with lap-delta + laps-remaining). Provides:
//   - Connectivity indicator on the left (telemetry light + 5G + bars)
//   - Optional center content (e.g. a fault summary)
//   - Screen label on the right (e.g. "DIAG", "SHUTDOWN", "SETTINGS")
//
// The host screen container needs `position: relative` and enough top
// padding (~70px) to keep its content from being hidden behind the tray.

interface TopTrayProps {
    screenLabel: string;
    children?: React.ReactNode;
}

const TopTray: React.FC<TopTrayProps> = ({ screenLabel, children }) => (
    <div className="top-tray">
        <div className="top-tray-section">
            <ConnectivityIndicator />
        </div>
        <div className="top-tray-section top-tray-center">{children}</div>
        <div className="top-tray-section top-tray-right">{screenLabel}</div>
    </div>
);

export default TopTray;
