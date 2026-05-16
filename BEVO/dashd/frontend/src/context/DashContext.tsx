import React, { createContext, useContext, useState, useEffect } from 'react';
import { DashMessage } from '../types/DashData';
import { useCarData } from '../hooks/useCarData';
import { useDemoData } from '../hooks/useDemoData';

interface DashContextValue {
    data: DashMessage | null;
    isConnected: boolean;
    isDemoMode: boolean;
}

const DashContext = createContext<DashContextValue>({
    data: null,
    isConnected: false,
    isDemoMode: false,
});

export const useDash = () => useContext(DashContext);

export const DashProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
    const [isDemoMode, setIsDemoMode] = useState(false);

    // Toggle demo mode with 'D' key
    useEffect(() => {
        const handleKeyDown = (e: KeyboardEvent) => {
            if (e.key === 'd' || e.key === 'D') {
                setIsDemoMode(prev => !prev);
            }
        };
        window.addEventListener('keydown', handleKeyDown);
        return () => window.removeEventListener('keydown', handleKeyDown);
    }, []);

    // Keep both data sources always-on. Toggling the WebSocket on/off via the
    // hook's enable flag had a stale-state quirk where `lastJsonMessage` stuck
    // at null after a re-enable, leaving the dash blank. Cheap to keep running.
    const live = useCarData(true);
    const demo = useDemoData(true);

    const data = isDemoMode ? demo : live.data;
    const isConnected = isDemoMode ? true : live.isConnected;

    return (
        <DashContext.Provider value={{ data, isConnected, isDemoMode }}>
            {/* Mode indicator (temporarily hidden) */}
            {false && (
                <div style={{
                    position: 'fixed',
                    top: '4px',
                    right: '8px',
                    zIndex: 9999,
                    fontSize: '0.6rem',
                    fontFamily: 'monospace',
                    color: isDemoMode ? '#FFD700' : (isConnected ? '#00FF66' : '#FF3333'),
                    opacity: 0.7,
                    pointerEvents: 'none',
                    letterSpacing: '1px',
                }}>
                    {isDemoMode ? 'DEMO' : (isConnected ? 'LIVE' : 'DISCONNECTED')}
                </div>
            )}
            {children}
        </DashContext.Provider>
    );
};
