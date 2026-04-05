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
    isDemoMode: true,
});

export const useDash = () => useContext(DashContext);

export const DashProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
    const [isDemoMode, setIsDemoMode] = useState(true);

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

    // Both hooks run, but only one is "enabled"
    const live = useCarData(!isDemoMode);
    const demo = useDemoData(isDemoMode);

    const data = isDemoMode ? demo : live.data;
    const isConnected = isDemoMode ? true : live.isConnected;

    return (
        <DashContext.Provider value={{ data, isConnected, isDemoMode }}>
            {/* Mode indicator */}
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
            {children}
        </DashContext.Provider>
    );
};
