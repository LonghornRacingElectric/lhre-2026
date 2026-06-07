import React, { useEffect, useRef, useState } from 'react';
import ScreenOne from './ScreenOne';
import ScreenTwo from './ScreenTwo';
import PitDiagnostic from './PitDiagnostic';
import Settings from './Settings';
import { useDash } from '../context/DashContext';

// Add new screens by appending to this array. Enter cycles in order.
const SCREENS: { name: string; component: React.FC }[] = [
    { name: 'Driving', component: ScreenOne },
    { name: 'PitDiagnostic', component: PitDiagnostic },
    { name: 'Shutdown', component: ScreenTwo },
    { name: 'Settings', component: Settings },
];

const DRIVING_INDEX = SCREENS.findIndex(s => s.name === 'Driving');
const PARK_INDEX = SCREENS.findIndex(s => s.name === 'PitDiagnostic');

const Dashboard: React.FC = () => {
    const [screenIndex, setScreenIndex] = useState<number>(0);
    const { data } = useDash();
    const prndl = data?.can.prndl ?? null;
    // Last gear we acted on, so we only snap screens on an actual P<->D
    // transition (not on every WS frame) and the crew can still Enter-cycle
    // freely between gear changes.
    const lastGearRef = useRef<string | null>(null);

    useEffect(() => {
        const handleKeyDown = (event: KeyboardEvent) => {
            if (event.key === 'Enter') {
                setScreenIndex(prev => (prev + 1) % SCREENS.length);
            }
        };
        window.addEventListener('keydown', handleKeyDown);
        return () => window.removeEventListener('keydown', handleKeyDown);
    }, []);

    // PRNDL-driven auto-routing: Park -> debug screen, Drive -> driving screen.
    // The VCU only emits P or D (PRNDL.h); null/unknown frames are ignored so a
    // dropped diag packet can't yank the screen. Fires only on a real gear
    // change, so the moment the driver shifts to Drive the dash leaves the pit
    // screen and shows the driving view — and shifting back to Park brings the
    // debug screen up again. Manual Enter navigation still works between shifts.
    useEffect(() => {
        if (prndl !== 'P' && prndl !== 'D') return;
        if (prndl === lastGearRef.current) return;
        lastGearRef.current = prndl;
        setScreenIndex(prndl === 'P' ? PARK_INDEX : DRIVING_INDEX);
    }, [prndl]);

    const ActiveScreen = SCREENS[screenIndex].component;

    return (
        <div style={{ width: '100vw', height: '100vh', overflow: 'hidden' }}>
            <ActiveScreen />
        </div>
    );
};

export default Dashboard;
