import { useEffect, useRef, useState } from 'react';
import { DashMessage } from '../types/DashData';

// Endurance energy pacing, computed entirely on-dash from the live CAN power
// stream plus two off-car signals (see BEVO/dashd/MQTT_CONTRACT.md):
//
//   - mqtt.targetPower (kW): the live power budget the trackside strategist
//     dials in. The dash integrates it over the same time steps it integrates
//     real power, so the comparison is apples-to-apples even across stalls.
//   - mqtt.lapTrigger (monotonic counter): bumped once per lap. Its rising
//     edge closes the current lap (snapshotting time + energy for the lap
//     card) and resets the per-lap integrators — so error can't accumulate
//     across laps (the per-lap reset Andrew asked for).
//
// Energy is integrated as Wh += power(kW) * dt(s) / 3.6, matching the
// trackside exporter's convention. Regen (negative power) credits back
// automatically.

// Ignore frame gaps longer than this when integrating — a stall (tab
// backgrounded, WS hiccup) shouldn't dump a huge slug of phantom energy.
const MAX_FRAME_DT_S = 0.5;

// How long the full-screen lap card stays up after a lap trigger.
export const LAP_CARD_MS = 5000;

export interface LapSummary {
    lapNumber: number;   // 1-based lap that just finished
    timeS: number;       // wall-clock lap duration in seconds
    energyWh: number;    // net energy used over the lap (drive minus regen)
}

export interface EnergyPacing {
    // Live per-lap integrals (reset on each lap trigger).
    lapEnergyWh: number;
    lapElapsedS: number;

    // used - budget for the current lap, in Wh. Positive = over budget (red),
    // negative = under budget / banking margin (green). null until a
    // targetPower signal is present.
    budgetDeltaWh: number | null;
    targetPowerKw: number | null;

    // 1-based number of the lap currently in progress.
    lapNumber: number;

    // The most recently completed lap, shown full-screen until cardUntilMs.
    // null once the card has timed out.
    lapCard: LapSummary | null;
}

const EMPTY: EnergyPacing = {
    lapEnergyWh: 0,
    lapElapsedS: 0,
    budgetDeltaWh: null,
    targetPowerKw: null,
    lapNumber: 1,
    lapCard: null,
};

export function useEnergyPacing(data: DashMessage | null): EnergyPacing {
    const [pacing, setPacing] = useState<EnergyPacing>(EMPTY);

    // Integration state lives in refs so 30 Hz frames don't thrash React.
    const lastSeq = useRef<number>(-1);
    const lastFrameMs = useRef<number | null>(null);
    const lapStartMs = useRef<number>(Date.now());
    const lapEnergyWh = useRef<number>(0);
    const lapBudgetWh = useRef<number>(0);
    const lapNumber = useRef<number>(1);
    const lastLapTrigger = useRef<number | null>(null);
    const cardUntilMs = useRef<number>(0);
    const lastCard = useRef<LapSummary | null>(null);

    useEffect(() => {
        if (!data) return;
        // Only act on genuinely new frames (the WS hook re-renders on other
        // state too); seq is monotonic per dashd connection.
        if (data.seq === lastSeq.current) return;
        lastSeq.current = data.seq;

        const now = Date.now();
        const power = data.can.power;
        const targetPower = data.mqtt.targetPower ?? null;
        const lapTrigger = data.mqtt.lapTrigger ?? null;

        // --- Integrate energy + budget over this frame ---
        if (lastFrameMs.current !== null && typeof power === 'number') {
            const dt = Math.min(MAX_FRAME_DT_S, (now - lastFrameMs.current) / 1000);
            if (dt > 0) {
                lapEnergyWh.current += (power * dt) / 3.6;
                if (typeof targetPower === 'number') {
                    lapBudgetWh.current += (targetPower * dt) / 3.6;
                }
            }
        }
        lastFrameMs.current = now;

        // --- Lap trigger: rising edge closes the lap ---
        // Initialise the baseline on first sight without firing, so a stale
        // value already on the broker at startup doesn't pop a phantom card.
        if (typeof lapTrigger === 'number') {
            if (lastLapTrigger.current === null) {
                lastLapTrigger.current = lapTrigger;
            } else if (lapTrigger > lastLapTrigger.current) {
                lastLapTrigger.current = lapTrigger;
                const summary: LapSummary = {
                    lapNumber: lapNumber.current,
                    timeS: (now - lapStartMs.current) / 1000,
                    energyWh: lapEnergyWh.current,
                };
                lastCard.current = summary;
                cardUntilMs.current = now + LAP_CARD_MS;
                // Reset per-lap integrators for the new lap.
                lapNumber.current += 1;
                lapStartMs.current = now;
                lapEnergyWh.current = 0;
                lapBudgetWh.current = 0;
            }
        }

        const card = now < cardUntilMs.current ? lastCard.current : null;

        setPacing({
            lapEnergyWh: lapEnergyWh.current,
            lapElapsedS: (now - lapStartMs.current) / 1000,
            budgetDeltaWh: targetPower === null ? null : lapEnergyWh.current - lapBudgetWh.current,
            targetPowerKw: targetPower,
            lapNumber: lapNumber.current,
            lapCard: card,
        });
    }, [data]);

    // Tick the card down even when no new frames arrive, so it always clears.
    useEffect(() => {
        if (!pacing.lapCard) return;
        const id = window.setTimeout(
            () => setPacing((p) => (Date.now() >= cardUntilMs.current ? { ...p, lapCard: null } : p)),
            LAP_CARD_MS,
        );
        return () => window.clearTimeout(id);
    }, [pacing.lapCard]);

    return pacing;
}
