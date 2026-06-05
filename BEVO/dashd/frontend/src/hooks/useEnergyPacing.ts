import { useEffect, useRef, useState } from 'react';
import { DashMessage } from '../types/DashData';

// Endurance energy pacing. The integration + lap detection now live on-car in
// dashd (BEVO/dashd/main.rs), so this hook is a thin consumer: it reads the
// authoritative pacing snapshot and only owns the transient full-screen lap
// card (show on each newly-completed lap, auto-clear after a few seconds).
//
// Why on-car: a chromium reload mid-endurance would otherwise zero the lap
// integrator, and the trackside mirror reads the exact same numbers dashd
// publishes to `lhre/dash/state`.

// How long the full-screen lap card stays up after a lap completes.
export const LAP_CARD_MS = 5000;

export interface LapSummary {
    lapNumber: number;   // the lap that just finished
    timeS: number;       // its duration in seconds
    energyWh: number;    // net energy used over the lap
}

export interface EnergyPacing {
    lapEnergyWh: number;
    lapElapsedS: number;
    budgetDeltaWh: number | null;  // >0 over budget (red), <0 under (green), null until a target is set
    targetPowerKw: number | null;
    lapNumber: number;             // 1-based lap in progress
    lapCard: LapSummary | null;    // the most recent completed lap, shown briefly
}

export function useEnergyPacing(data: DashMessage | null): EnergyPacing {
    const [lapCard, setLapCard] = useState<LapSummary | null>(null);
    // Highest completed-lap number we've already reacted to. Seeded on first
    // sight so a reload mid-session doesn't pop a card for an old lap.
    const lastShownLap = useRef<number | null>(null);

    const pacing = data?.pacing;
    const lastLapNumber = pacing?.lastLapNumber ?? null;
    const lastLapTimeS = pacing?.lastLapTimeS ?? null;
    const lastLapEnergyWh = pacing?.lastLapEnergyWh ?? null;

    useEffect(() => {
        if (lastLapNumber === null) return;
        // First observation just sets the baseline (no card).
        if (lastShownLap.current === null) {
            lastShownLap.current = lastLapNumber;
            return;
        }
        if (lastLapNumber > lastShownLap.current) {
            lastShownLap.current = lastLapNumber;
            setLapCard({
                lapNumber: lastLapNumber,
                timeS: lastLapTimeS ?? 0,
                energyWh: lastLapEnergyWh ?? 0,
            });
        }
    }, [lastLapNumber, lastLapTimeS, lastLapEnergyWh]);

    // Auto-clear the card after LAP_CARD_MS.
    useEffect(() => {
        if (!lapCard) return;
        const id = window.setTimeout(() => setLapCard(null), LAP_CARD_MS);
        return () => window.clearTimeout(id);
    }, [lapCard]);

    return {
        lapEnergyWh: pacing?.lapEnergyWh ?? 0,
        lapElapsedS: pacing?.lapElapsedS ?? 0,
        budgetDeltaWh: pacing?.budgetDeltaWh ?? null,
        targetPowerKw: data?.mqtt.targetPower ?? null,
        lapNumber: pacing?.lapNumber ?? 1,
        lapCard,
    };
}
