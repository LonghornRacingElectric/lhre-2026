import { useState, useEffect, useRef } from 'react';
import { DashMessage } from '../types/DashData';

interface SimState {
    phase: 'accel' | 'cruise' | 'brake';
    speed: number;
    targetSpeed: number;
    accelRate: number;
    timer: number;
}

export function useDemoData(enabled: boolean): DashMessage | null {
    const [message, setMessage] = useState<DashMessage | null>(null);
    const seq = useRef(0);

    // Persistent simulation state
    const sim = useRef<SimState>({
        phase: 'accel',
        speed: 0,
        targetSpeed: 60,
        accelRate: 0.5,
        timer: 0,
    });

    // Persistent accumulator state (not reset each tick)
    const accum = useRef({
        charge: 100,
        temp: 32,         // warm garage, below cruise band
        odometer: 101.3,
        signalStrength: 4,
        shutdown: Array(16).fill(true) as boolean[],

        // Lap timer state. currentLapStart is wall-clock; bestLap / lastLap
        // are seconds. lapTarget is randomized per lap to give variation.
        bestLap: 75.30,
        lastLap: 78.45,
        currentLapStart: Date.now(),
        lapTarget: 75 + Math.random() * 8,
    });

    useEffect(() => {
        if (!enabled) return;

        const interval = setInterval(() => {
            const s = sim.current;
            const a = accum.current;
            let newPower = 0;

            // --- Physics simulation (from ScreenOne) ---
            if (s.phase === 'accel') {
                if (s.speed < s.targetSpeed) {
                    s.speed += s.accelRate;
                    newPower = 20 + (s.accelRate * 50) + (s.speed / 100 * 30);
                } else {
                    s.phase = 'cruise';
                    s.timer = 20 + Math.random() * 50;
                }
            } else if (s.phase === 'cruise') {
                s.timer--;
                s.speed += (Math.random() - 0.5) * 0.1;
                newPower = 10 + (s.speed / 100 * 20) + (Math.random() * 2);

                if (s.timer <= 0) {
                    if (s.speed > 50 && Math.random() > 0.3) {
                        s.phase = 'brake';
                        s.targetSpeed = Math.random() * 20;
                        s.accelRate = 0.5 + Math.random() * 1.0;
                    } else {
                        s.phase = 'accel';
                        s.targetSpeed = Math.min(99, s.speed + 20 + Math.random() * 30);
                        s.accelRate = 0.2 + Math.random() * 0.6;
                    }
                }
            } else if (s.phase === 'brake') {
                if (s.speed > s.targetSpeed) {
                    s.speed -= s.accelRate;
                    newPower = -5 - (s.accelRate * 20);
                } else {
                    s.phase = 'accel';
                    s.targetSpeed = 40 + Math.random() * 40;
                    s.accelRate = 0.3 + Math.random() * 0.5;
                }
            }

            s.speed = Math.max(0, Math.min(s.speed, 99));
            const clampedPower = Math.min(Math.max(newPower, -80), 80);

            // Accumulate derived values
            a.charge = Math.max(0, a.charge - (newPower > 0 ? 0.005 : -0.001));

            // Cell temperature: physics-style. Heated by total power
            // throughput (drive AND regen — both pump current through the
            // pack), cooled toward ambient. Calibrated so cruise lap sits
            // ~35–42°C and a hard run with full braking creeps toward but
            // does not exceed the 60°C cell limit.
            const ambient = 25;
            const heatIn = Math.abs(clampedPower) * 0.0005;
            const heatLoss = (a.temp - ambient) * 0.0012;
            a.temp = Math.min(60, Math.max(ambient, a.temp + heatIn - heatLoss));

            // Odometer pinned for demo — leave a.odometer untouched.

            // Signal strength: occasional random changes
            if (Math.random() > 0.95) {
                a.signalStrength = Math.floor(Math.random() * 5);
            }

            // Shutdown circuit stays fully OK in demo mode — no random
            // flips, otherwise the dash trips constantly.
            for (let i = 0; i < 16; i++) a.shutdown[i] = true;

            // MQTT-like derived values
            const lapDelta = parseFloat((Math.sin(Date.now() / 1000) * 2).toFixed(2));
            // s/s is the analytical derivative of the sin wave — gives a
            // smooth ±0.002 oscillation, far below the bar's ±0.5 max. To
            // make the bar more visible in demo, scale it up.
            const lapDeltaRate = Math.cos(Date.now() / 1000) * 0.3;
            const energyDelta = parseFloat((Math.cos(Date.now() / 1000) * 5).toFixed(1));

            // Laps remaining calculations
            const baseConsumption = 4.0;
            const fluctuation = (Math.sin(Date.now() / 2000) * 0.5) + (Math.random() * 0.2);
            const currentConsumption = baseConsumption + fluctuation;
            const lapsRemaining = currentConsumption > 0 ? a.charge / currentConsumption : 0;
            // Energy-based: slightly more conservative than session count.
            const lapsRemainingEnergy = lapsRemaining * 0.85;

            // Lap timer: tick from currentLapStart; on completion, save as
            // last lap, update best if it beat it, reset for next lap.
            let elapsed = (Date.now() - a.currentLapStart) / 1000;
            if (elapsed >= a.lapTarget) {
                a.lastLap = elapsed;
                if (elapsed < a.bestLap) a.bestLap = elapsed;
                a.currentLapStart = Date.now();
                a.lapTarget = 75 + Math.random() * 8;
                elapsed = 0;
            }
            const currentLapTime = elapsed;

            // Brake bias: slow drift around 55%, range ~52–58%.
            const brakeBias = 55 + Math.sin(Date.now() / 5000) * 3;

            // Driver-armable flags — placeholders until backend wires them.
            // Cycle through states slowly so both enabled/disabled UI is
            // visible during a casual look at the dash.
            const tcLevel = (Math.floor(Date.now() / 5000) % 3) + 1;        // 1,2,3
            const tcEnabled = (Math.floor(Date.now() / 9000) % 3) !== 0;     // ~66% on
            const regenEnabled = (Math.floor(Date.now() / 8000) % 4) !== 0;  // ~75% on

            // Pit-diagnostic fakes — derived from the existing sim where
            // possible so values move with the rest of the dash.
            const apps = s.phase === 'accel'
                ? Math.min(100, s.accelRate * 80 + Math.random() * 5)
                : s.phase === 'cruise'
                    ? 12 + Math.random() * 6
                    : 0;
            const bpps = s.phase === 'brake'
                ? Math.min(100, s.accelRate * 45 + Math.random() * 5)
                : 0;
            // Brake pressures: ~700 psi at 100% bpps, split by brakeBias.
            const brakePressureFront = bpps * 7 * (brakeBias / 100);
            const brakePressureRear = bpps * 7 * (1 - brakeBias / 100);
            // Powertrain temps — ambient baseline + |power|-driven heating.
            const motorTemp = Math.min(120, 50 + Math.abs(clampedPower) * 0.4);
            const inverterTemp = Math.min(90, 45 + Math.abs(clampedPower) * 0.25);
            const coolantTemp = Math.min(70, 35 + Math.abs(clampedPower) * 0.15);
            // Cell stats — max already comes from a.temp; avg/min are
            // synthesized just below it.
            const cellTempAvg = a.temp - 1.5;
            const cellTempMin = a.temp - 3.5;
            // Bottom tray pinned to plausible static values for the design
            // briefing demo. Live values will come from dashd once wired.
            const hvVoltage = 449.3;
            const hvCurrent = 10;
            const lvVoltage = 25;
            const lvCurrent = 9.3;
            // Wheel speeds — small per-wheel variance for visual interest.
            const wheelSpeedFL = Math.max(0, s.speed + (Math.random() - 0.5) * 0.4);
            const wheelSpeedFR = Math.max(0, s.speed + (Math.random() - 0.5) * 0.4);
            const wheelSpeedRL = Math.max(0, s.speed + (Math.random() - 0.5) * 0.6);
            const wheelSpeedRR = Math.max(0, s.speed + (Math.random() - 0.5) * 0.6);

            // PRNDL: park while stopped, drive once moving. Toy logic.
            const prndl: string | null = s.speed > 1 ? 'D' : 'P';
            // Contactors in demo mode: pretend HV is energized once we're
            // in drive; cycle through a brief precharge state at startup.
            const posContactor = s.speed > 1;
            const prechargeContactor = !posContactor && Math.floor(Date.now() / 4000) % 2 === 0;
            const negContactor = true; // shutdown circuit closed in demo

            seq.current++;

            setMessage({
                seq: seq.current,
                can: {
                    speed: s.speed,
                    power: clampedPower,
                    odometer: a.odometer,
                    soc: a.charge,
                    temperature: a.temp,
                    signalStrength: a.signalStrength,
                    shutdown: [...a.shutdown],
                    prndl,
                    posContactor,
                    negContactor,
                    prechargeContactor,
                    brakeBias,
                    tcLevel,
                    tcEnabled,
                    regenEnabled,
                    apps,
                    bpps,
                    brakePressureFront,
                    brakePressureRear,
                    motorTemp,
                    inverterTemp,
                    coolantTemp,
                    cellTempAvg,
                    cellTempMin,
                    hvVoltage,
                    hvCurrent,
                    lvVoltage,
                    lvCurrent,
                    wheelSpeedFL,
                    wheelSpeedFR,
                    wheelSpeedRL,
                    wheelSpeedRR,
                },
                mqtt: {
                    lapDelta,
                    energyDelta,
                    lapsRemaining,
                    lapsRemainingEnergy,
                    bestLapTime: a.bestLap,
                    lastLapTime: a.lastLap,
                    currentLapTime,
                    lapDeltaRate,
                },
            });
        }, 100);

        return () => clearInterval(interval);
    }, [enabled]);

    return enabled ? message : null;
}
