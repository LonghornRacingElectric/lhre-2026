import React, { useEffect, useRef, useState } from 'react';
import ScreenOne from './ScreenOne';
import ScreenTwo from './ScreenTwo';
import PitDiagnostic from './PitDiagnostic';
import Settings from './Settings';
import { useDash } from '../context/DashContext';
import { MessageCardRenderer } from '../MessageCardRenderer';
import { validateMessage } from '../dashMessages';
import { LapCardOverlay } from '../components/LapCardOverlay';

// Add new screens by appending to this array. Enter cycles in order.
const SCREENS: { name: string; component: React.FC }[] = [
    { name: 'Driving', component: ScreenOne },
    { name: 'PitDiagnostic', component: PitDiagnostic },
    { name: 'Shutdown', component: ScreenTwo },
    { name: 'Settings', component: Settings },
];

const DRIVING_INDEX = SCREENS.findIndex(s => s.name === 'Driving');
const PARK_INDEX = SCREENS.findIndex(s => s.name === 'PitDiagnostic');

// Trackside debug screen-override names -> SCREENS index. The website publishes
// one of these on lhre/dash/screen; dashd forwards it as screenOverride and
// clears it on a real gear change. Unknown names fall through to PRNDL routing.
const OVERRIDE_INDEX: Record<string, number> = {
    driving: DRIVING_INDEX,
    park: PARK_INDEX,
    shutdown: SCREENS.findIndex(s => s.name === 'Shutdown'),
    settings: SCREENS.findIndex(s => s.name === 'Settings'),
};

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

    // A trackside debug override (if one names a known screen) takes precedence
    // over PRNDL/Enter routing without disturbing screenIndex — so when it
    // clears (released from the website, or auto-cleared by dashd on a gear
    // change) the dash snaps right back to whatever it was showing.
    const override = data?.screenOverride ?? null;
    const overrideIndex = override != null ? OVERRIDE_INDEX[override] : undefined;
    const effectiveIndex = overrideIndex != null && overrideIndex >= 0 ? overrideIndex : screenIndex;

    const ActiveScreen = SCREENS[effectiveIndex].component;

    // Driver-message overlay floats on top of WHATEVER screen is active — Drive,
    // Park/PitDiagnostic (built-in OR custom layout), Shutdown, Settings — so a
    // strategist nudge is never hidden behind the current screen. Lifted here
    // from ScreenOne so it is rendered exactly once, for every screen.
    const driverMsg = validateMessage(data?.message);
    const dashTheme: 'light' | 'dark' =
        (typeof document !== 'undefined' && document.body.classList.contains('theme-light')) ? 'light' : 'dark';

    return (
        <div style={{ width: '100vw', height: '100vh', overflow: 'hidden', position: 'relative' }}>
            <ActiveScreen />
            {/* Lap card (z-1000) floats over every screen — built-in or custom driving
                layout, Park, etc. — lifted out of ScreenOne so it isn't tied to one screen. */}
            <LapCardOverlay data={data} theme={dashTheme} />
            {driverMsg ? (
                <div style={{ position: 'absolute', inset: 0, zIndex: 1100 }}>
                    <MessageCardRenderer message={driverMsg} theme={dashTheme} scale={1} />
                </div>
            ) : null}
        </div>
    );
};

export default Dashboard;
