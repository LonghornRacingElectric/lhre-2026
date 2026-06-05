'use client';

import mqtt, { type MqttClient } from 'mqtt';
import { useCallback, useEffect, useRef, useState } from 'react';

// Browser-side publisher for the on-car dash's pacing signals. The dash
// (BEVO/dashd) subscribes to `lhre/dash/#` on the same broker the telemetry
// stack uses; that broker exposes a websockets listener on :8080
// (telemtry/stack/ingest/mosquitto.conf), so we can publish straight from the
// strategist's browser with no extra backend. Contract: BEVO/dashd/MQTT_CONTRACT.md.

const TOPIC_PREFIX = 'lhre/dash/';
const DEFAULT_BROKER_URL = 'ws://18.191.225.118:8080';
const BROKER_STORAGE_KEY = 'dash-signal-broker';

// targetPower is a set-point, not a stream. dashd nulls any field it hasn't
// heard from in 5 s, so we re-publish the current target on this cadence to
// keep the dash's energy bar alive between manual changes.
const KEEPALIVE_MS = 1000;

export type DashSignalStatus = 'idle' | 'connecting' | 'connected' | 'error' | 'closed';

// Mirror of dashd's DashStateMsg (lhre/dash/state) — what the driver currently
// sees, plus the lap count, so trackside can confirm the uplink.
export interface DashMirrorState {
    lapCount: number;
    targetPower: number | null;
    targetPowerStale: boolean;
    speed: number | null;
    power: number | null;
    soc: number | null;
    temperature: number | null;
    pacing: {
        lapEnergyWh: number;
        budgetDeltaWh: number | null;
        lapElapsedS: number;
        lapNumber: number;
        lastLapNumber: number | null;
        lastLapTimeS: number | null;
        lastLapEnergyWh: number | null;
    };
}

// Last value the car echoed back on lhre/dash/ack/*, with arrival time, so the
// strategist knows the signal landed (not just that we sent it).
export interface DashAcks {
    targetPower?: { value: number; at: number };
    lapTrigger?: { value: number; at: number };
    sfGate?: { at: number };
}

export interface DashSignals {
    status: DashSignalStatus;
    error: string | null;
    brokerUrl: string;
    setBrokerUrl: (url: string) => void;
    lapsSent: number;
    connect: () => void;
    disconnect: () => void;
    /** Set the live power budget (kW) shown on the dash energy bar. */
    publishTargetPower: (kw: number) => void;
    /** Bump the lap counter — fires the dash's full-screen lap card + per-lap reset. */
    sendLap: () => void;
    /** Push the start/finish gate [lat1,lon1,lat2,lon2] so the car counts laps itself (retained). */
    publishGate: (gate: [number, number, number, number]) => void;
    /** Latest mirror of the driver's screen (null until the car publishes state). */
    dashState: DashMirrorState | null;
    /** Wall-clock ms of the last lhre/dash/state message — for a link-silent warning. */
    lastStateAt: number | null;
    /** Last echoed control values, so trackside can confirm the car heard us. */
    acks: DashAcks;
}

function loadBrokerUrl(): string {
    if (typeof window === 'undefined') return DEFAULT_BROKER_URL;
    return localStorage.getItem(BROKER_STORAGE_KEY) || DEFAULT_BROKER_URL;
}

export function useDashSignals(): DashSignals {
    const [status, setStatus] = useState<DashSignalStatus>('idle');
    const [error, setError] = useState<string | null>(null);
    const [brokerUrl, setBrokerUrlState] = useState<string>(loadBrokerUrl);
    const [lapsSent, setLapsSent] = useState(0);
    const [dashState, setDashState] = useState<DashMirrorState | null>(null);
    const [lastStateAt, setLastStateAt] = useState<number | null>(null);
    const [acks, setAcks] = useState<DashAcks>({});

    const clientRef = useRef<MqttClient | null>(null);
    const targetPowerRef = useRef<number | null>(null);
    const lapCounterRef = useRef(0);
    const keepaliveRef = useRef<number | null>(null);

    const publish = useCallback((field: string, value: number) => {
        const client = clientRef.current;
        if (!client || !client.connected) return;
        client.publish(`${TOPIC_PREFIX}${field}`, String(value), { qos: 0 });
    }, []);

    const disconnect = useCallback(() => {
        if (keepaliveRef.current !== null) {
            window.clearInterval(keepaliveRef.current);
            keepaliveRef.current = null;
        }
        const client = clientRef.current;
        clientRef.current = null;
        if (client) client.end(true);
        setStatus('closed');
    }, []);

    const connect = useCallback(() => {
        // Tear down any prior client before reconnecting.
        if (clientRef.current) {
            clientRef.current.end(true);
            clientRef.current = null;
        }
        if (keepaliveRef.current !== null) {
            window.clearInterval(keepaliveRef.current);
            keepaliveRef.current = null;
        }

        setError(null);
        setStatus('connecting');
        const clientId = `dash-signal-sender-${Math.floor(Math.random() * 1e6)}`;
        let client: MqttClient;
        try {
            client = mqtt.connect(brokerUrl, { clientId, reconnectPeriod: 2000, connectTimeout: 8000 });
        } catch (e) {
            setStatus('error');
            setError(e instanceof Error ? e.message : String(e));
            return;
        }
        clientRef.current = client;

        client.on('connect', () => {
            setStatus('connected');
            setError(null);
            // Subscribe to the car's mirror + acks so trackside can see what the
            // driver sees and confirm the uplink.
            client.subscribe([`${TOPIC_PREFIX}state`, `${TOPIC_PREFIX}ack/#`]);
            // Push the current target straight away so a reconnect doesn't
            // leave the dash bar blank until the next keepalive tick.
            if (targetPowerRef.current !== null) publish('targetPower', targetPowerRef.current);
        });
        client.on('reconnect', () => setStatus('connecting'));
        client.on('close', () => setStatus((s) => (s === 'error' ? s : 'closed')));
        client.on('error', (err) => {
            setStatus('error');
            setError(err instanceof Error ? err.message : String(err));
        });
        client.on('message', (topic, payload) => {
            const text = payload.toString();
            if (topic === `${TOPIC_PREFIX}state`) {
                try {
                    setDashState(JSON.parse(text) as DashMirrorState);
                    setLastStateAt(Date.now());
                } catch {
                    // ignore malformed state frame
                }
            } else if (topic === `${TOPIC_PREFIX}ack/targetPower`) {
                const v = Number(text);
                if (Number.isFinite(v)) setAcks((a) => ({ ...a, targetPower: { value: v, at: Date.now() } }));
            } else if (topic === `${TOPIC_PREFIX}ack/lapTrigger`) {
                const v = Number(text);
                if (Number.isFinite(v)) setAcks((a) => ({ ...a, lapTrigger: { value: v, at: Date.now() } }));
            } else if (topic === `${TOPIC_PREFIX}ack/sfGate`) {
                setAcks((a) => ({ ...a, sfGate: { at: Date.now() } }));
            }
        });

        keepaliveRef.current = window.setInterval(() => {
            if (targetPowerRef.current !== null) publish('targetPower', targetPowerRef.current);
        }, KEEPALIVE_MS);
    }, [brokerUrl, publish]);

    const publishTargetPower = useCallback((kw: number) => {
        targetPowerRef.current = kw;
        publish('targetPower', kw);
    }, [publish]);

    const sendLap = useCallback(() => {
        lapCounterRef.current += 1;
        publish('lapTrigger', lapCounterRef.current);
        setLapsSent(lapCounterRef.current);
    }, [publish]);

    const publishGate = useCallback((gate: [number, number, number, number]) => {
        const client = clientRef.current;
        if (!client || !client.connected) return;
        // Retained + QoS 1: the car re-loads the current gate on every reconnect,
        // and a freshly-booted dash gets it without the strategist re-sending.
        client.publish(`${TOPIC_PREFIX}sfGate`, JSON.stringify(gate), { qos: 1, retain: true });
    }, []);

    const setBrokerUrl = useCallback((url: string) => {
        setBrokerUrlState(url);
        if (typeof window !== 'undefined') localStorage.setItem(BROKER_STORAGE_KEY, url);
    }, []);

    // Clean up the client + keepalive when the component unmounts.
    useEffect(() => () => {
        if (keepaliveRef.current !== null) window.clearInterval(keepaliveRef.current);
        clientRef.current?.end(true);
    }, []);

    return {
        status,
        error,
        brokerUrl,
        setBrokerUrl,
        lapsSent,
        connect,
        disconnect,
        publishTargetPower,
        sendLap,
        publishGate,
        dashState,
        lastStateAt,
        acks,
    };
}
