import React, { useEffect, useState } from 'react';
import ScreenOne from './ScreenOne';
import ScreenTwo from './ScreenTwo';
import PitDiagnostic from './PitDiagnostic';
import Settings from './Settings';

// Add new screens by appending to this array. Enter cycles in order.
const SCREENS: { name: string; component: React.FC }[] = [
    { name: 'Driving', component: ScreenOne },
    { name: 'PitDiagnostic', component: PitDiagnostic },
    { name: 'Shutdown', component: ScreenTwo },
    { name: 'Settings', component: Settings },
];

const Dashboard: React.FC = () => {
    const [screenIndex, setScreenIndex] = useState<number>(0);

    useEffect(() => {
        const handleKeyDown = (event: KeyboardEvent) => {
            if (event.key === 'Enter') {
                setScreenIndex(prev => (prev + 1) % SCREENS.length);
            }
        };
        window.addEventListener('keydown', handleKeyDown);
        return () => window.removeEventListener('keydown', handleKeyDown);
    }, []);

    const ActiveScreen = SCREENS[screenIndex].component;

    return (
        <div style={{ width: '100vw', height: '100vh', overflow: 'hidden' }}>
            <ActiveScreen />
        </div>
    );
};

export default Dashboard;
