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
    lapBudgetWh: number | null;    // live per-lap budget target (Wh) from trackside
    targetPowerKw: number | null;
    lapNumber: number;             // 1-based lap in progress
    lapCard: LapSummary | null;    // the most recent completed lap, shown briefly
}

export function useEnergyPacing(data: DashMessage | null): EnergyPacing {
    const [lapCard, setLapCard] = useState<LapSummary | null>(null);
    // Highest completed-lap number we've already reacted to. null until we've
    // seen our first WS frame, then either a number (mid-session reload —
    // adopt as baseline, no card) or null (post-reset — any non-null rise
    // fires a card for the first lap of the new session).
    const lastShownLap = useRef<number | null>(null);
    // True once we've adopted the baseline from the first real WS frame.
    // Distinguishes "mounted, no data yet" from "post-reset, dashd reports
    // null". Without this, the first non-null lap number after mount would
    // be treated as a fresh-session lap 1 and fire a stale card on reload.
    const initializedRef = useRef(false);

    const pacing = data?.pacing;
    const lastLapNumber = pacing?.lastLapNumber ?? null;
    const lastLapTimeS = pacing?.lastLapTimeS ?? null;
    const lastLapEnergyWh = pacing?.lastLapEnergyWh ?? null;

    useEffect(() => {
        // Hold off until the first real WS frame so we can baseline against
        // dashd's actual state (null = fresh, N = mid-session) rather than
        // the pre-connection null.
        if (data == null) return;
        if (!initializedRef.current) {
            initializedRef.current = true;
            lastShownLap.current = lastLapNumber; // can be null
            return;
        }
        if (lastLapNumber === null) {
            // dashd cleared its last_lap (session reset). Drop our baseline so
            // the next non-null lap fires a card — every session's first lap
            // included.
            lastShownLap.current = null;
            return;
        }
        const baseline = lastShownLap.current;
        if (baseline === null || lastLapNumber > baseline) {
            lastShownLap.current = lastLapNumber;
            setLapCard({
                lapNumber: lastLapNumber,
                timeS: lastLapTimeS ?? 0,
                energyWh: lastLapEnergyWh ?? 0,
            });
        }
    }, [data, lastLapNumber, lastLapTimeS, lastLapEnergyWh]);

    // Auto-clear the card after the trackside-set duration (lapCardMs), falling
    // back to the built-in default. Clamped to a sane range so a bad value can't
    // pin the card forever or flash it away.
    const lapCardMs = Math.min(30000, Math.max(1000, data?.mqtt?.lapCardMs ?? LAP_CARD_MS));
    useEffect(() => {
        if (!lapCard) return;
        const id = window.setTimeout(() => setLapCard(null), lapCardMs);
        return () => window.clearTimeout(id);
    }, [lapCard, lapCardMs]);

    return {
        lapEnergyWh: pacing?.lapEnergyWh ?? 0,
        lapElapsedS: pacing?.lapElapsedS ?? 0,
        budgetDeltaWh: pacing?.budgetDeltaWh ?? null,
        lapBudgetWh: pacing?.lapBudgetWh ?? null,
        targetPowerKw: data?.mqtt.targetPower ?? null,
        lapNumber: pacing?.lapNumber ?? 1,
        lapCard,
    };
}
