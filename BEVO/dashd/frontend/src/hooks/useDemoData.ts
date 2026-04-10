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
        temp: 40,
        odometer: 90.5,
        signalStrength: 4,
        shutdown: Array(16).fill(true) as boolean[],
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
                        s.targetSpeed = Math.min(100, s.speed + 20 + Math.random() * 30);
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

            s.speed = Math.max(0, Math.min(s.speed, 100));
            const clampedPower = Math.min(Math.max(newPower, -80), 80);

            // Accumulate derived values
            a.charge = Math.max(0, a.charge - (newPower > 0 ? 0.05 : -0.01));
            a.temp = Math.min(100, Math.max(20, a.temp + (newPower > 50 ? 0.05 : -0.02)));
            a.odometer += s.speed / 3600 / 10;

            // Signal strength: occasional random changes
            if (Math.random() > 0.95) {
                a.signalStrength = Math.floor(Math.random() * 5);
            }

            // Shutdown circuit: occasional random flips
            if (Math.random() > 0.8) {
                const idx = Math.floor(Math.random() * 16);
                a.shutdown[idx] = !a.shutdown[idx];
            } else {
                const idx = Math.floor(Math.random() * 16);
                a.shutdown[idx] = true;
            }

            // MQTT-like derived values
            const lapDelta = parseFloat((Math.sin(Date.now() / 1000) * 2).toFixed(2));
            const energyDelta = parseFloat((Math.cos(Date.now() / 1000) * 5).toFixed(1));

            // Laps remaining calculation
            const baseConsumption = 4.0;
            const fluctuation = (Math.sin(Date.now() / 2000) * 0.5) + (Math.random() * 0.2);
            const currentConsumption = baseConsumption + fluctuation;
            const lapsRemaining = currentConsumption > 0 ? a.charge / currentConsumption : 0;

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
                },
                mqtt: {
                    lapDelta,
                    energyDelta,
                    lapsRemaining,
                },
            });
        }, 100);

        return () => clearInterval(interval);
    }, [enabled]);

    return enabled ? message : null;
}
