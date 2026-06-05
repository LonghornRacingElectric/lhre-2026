import React from 'react';
import { useDash } from '../context/DashContext';

// Telemetry / 5G connectivity indicator. Single source of truth for any
// screen that needs the "5G + signal bars + telemetry light" cluster —
// pulls from useDash() so all instances stay in sync automatically.
//
// Telemetry status: GREEN if any MQTT field (lapDelta / energyDelta /
// lapsRemaining) is non-null, RED otherwise.
// Signal bars: 0–4, from CanData.signalStrength (currently always null
// upstream — bars render empty until the 5G hardware is integrated).

const ConnectivityIndicator: React.FC = () => {
    const { data } = useDash();

    const lapDelta = data?.mqtt.lapDelta;
    const energyDelta = data?.mqtt.energyDelta;
    const lapsRemaining = data?.mqtt.lapsRemaining;
    const signalStrength = data?.can.signalStrength;

    const telemetryStatus =
        lapDelta !== null || energyDelta !== null || lapsRemaining !== null;
    const safeSignal = signalStrength ?? 0;

    return (
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            {/* Telemetry status light */}
            <div
                style={{
                    width: '10px',
                    height: '10px',
                    borderRadius: '50%',
                    backgroundColor: telemetryStatus ? '#00FF66' : '#FF3333',
                    boxShadow: telemetryStatus
                        ? '0 0 8px #00FF66'
                        : '0 0 8px #FF3333',
                }}
            />
            <div
                className="label-small"
                style={{ fontSize: '0.8rem', marginBottom: 0, color: 'var(--fg-secondary)' }}
            >
                5G
            </div>
            {/* Signal bars */}
            <div
                style={{
                    display: 'flex',
                    alignItems: 'flex-end',
                    gap: '3px',
                    height: '14px',
                }}
            >
                {[1, 2, 3, 4].map(bar => (
                    <div
                        key={bar}
                        style={{
                            width: '4px',
                            height: `${bar * 25}%`,
                            backgroundColor:
                                bar <= safeSignal ? 'var(--fg-primary)' : 'var(--bar-track)',
                            borderRadius: '1px',
                        }}
                    />
                ))}
            </div>
        </div>
    );
};

export default ConnectivityIndicator;
