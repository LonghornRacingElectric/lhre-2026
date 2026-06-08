import React from 'react';
import { LapCardRenderer } from '../LapCardRenderer';
import { validateLapCardLayout } from '../dashLayout';
import { useEnergyPacing } from '../hooks/useEnergyPacing';
import type { DashMessage } from '../types/DashData';

const fmtLapTime = (secs: number | null | undefined): string => {
    if (secs === null || secs === undefined) return "--:--.--";
    const m = Math.floor(secs / 60);
    const s = secs - m * 60;
    return `${m}:${s.toFixed(2).padStart(5, '0')}`;
};

// Full-screen lap card — pops on each lapTrigger crossing and clears itself after
// LAP_CARD_MS (see useEnergyPacing). Rendered by Dashboard (not a single screen) so
// it floats over WHATEVER screen is active — built-in OR custom driving layout, Park,
// Shutdown — exactly like the driver-message overlay. It owns its own useEnergyPacing
// instance; that hook is driven purely by mqtt.lapTrigger, so it stays in sync with
// any other instance (e.g. ScreenOne's energy readout) without shared state.
//
// z-index 1000: below the driver-message overlay (1100) so a strategist nudge still
// wins, above all screen content.
export function LapCardOverlay({ data, theme }: { data: DashMessage | null; theme: 'light' | 'dark' }) {
    const pacing = useEnergyPacing(data);
    if (!pacing.lapCard) return null;

    // Trackside custom lap-card layout, if one was sent; else the built-in card.
    // validateLapCardLayout returns null for a missing/malformed layout, so the
    // driver screen never blanks.
    const customLayout = validateLapCardLayout(data?.layout);
    if (customLayout && customLayout.widgets.length) {
        const ctx = {
            lapCard: pacing.lapCard,
            pacing: data?.pacing,
            can: data?.can,
            mqtt: data?.mqtt,
        };
        return (
            <div style={{ position: 'absolute', inset: 0, zIndex: 1000 }}>
                <LapCardRenderer layout={customLayout} data={ctx} scale={1} theme={theme} />
            </div>
        );
    }
    return (
        <div style={{
            position: 'absolute',
            inset: 0,
            zIndex: 1000,
            background: theme === 'light' ? 'rgba(244,241,237,0.94)' : 'rgba(8,8,10,0.92)',
            backdropFilter: 'blur(6px)',
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '8px'
        }}>
            <div className="label-small" style={{ fontSize: '1.4rem', letterSpacing: '8px', color: '#BF5700' }}>
                LAP {pacing.lapCard.lapNumber}
            </div>
            <div className="value-display" style={{ fontSize: '7rem', fontWeight: 'bold', lineHeight: 0.9 }}>
                {fmtLapTime(pacing.lapCard.timeS)}
            </div>
            <div className="value-display" style={{ fontSize: '3rem', fontWeight: 'bold', color: 'var(--fg-secondary)', lineHeight: 1 }}>
                {Math.round(pacing.lapCard.energyWh)}
                <span className="label-small" style={{ fontSize: '1.1rem', marginLeft: '8px' }}>Wh / LAP</span>
            </div>
        </div>
    );
}

export default LapCardOverlay;
