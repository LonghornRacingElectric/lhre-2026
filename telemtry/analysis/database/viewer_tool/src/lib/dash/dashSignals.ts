'use client';

import mqtt, { type MqttClient } from 'mqtt';
import { useCallback, useEffect, useRef, useState } from 'react';
import type { DashMessage, MessageTrigger } from './dashMessages';

// Browser-side publisher for the on-car dash's pacing signals. The dash
// (BEVO/dashd) subscribes to `lhre/dash/#` on the same broker the telemetry
// stack uses; that broker exposes a websockets listener on :8080
// (telemtry/stack/ingest/mosquitto.conf), so we can publish straight from the
// strategist's browser with no extra backend. Contract: BEVO/dashd/MQTT_CONTRACT.md.

const TOPIC_PREFIX = 'lhre/dash/';
const BROKER_STORAGE_KEY = 'dash-signal-broker';

// The dash broker is reached over WebSockets. On an HTTPS page the browser
// blocks an insecure ws:// connection (mixed content) — which silently fails as
// "connecting → disconnecting" — so default to a same-origin wss path that the
// reverse proxy forwards to the mosquitto ws listener. Plain-http (local dev)
// keeps the direct ws:// default. Override-able via the Dash tab field.
const DEV_BROKER_URL = 'ws://18.191.225.118:8080';
function defaultBrokerUrl(): string {
    if (typeof window === 'undefined') return DEV_BROKER_URL;
    return window.location.protocol === 'https:'
        ? `wss://${window.location.host}/mqtt`
        : DEV_BROKER_URL;
}

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
    // Car storage + runtime vs the on-car kill switch (free < 1 GB or > 1 h
    // stops telemetry). null until the car's dashd reports them.
    diskFreeMb?: number | null;
    diskTotalMb?: number | null;
    runtimeS?: number | null;
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
    sendLap: (value?: number) => void;
    /** Re-publish a lap value without bumping the internal counter — for the drain
     *  effect that catches up the car after a missed publish. dashd's forward-only
     *  edge check makes a duplicate publish harmless. */
    republishLap: (value: number) => void;
    resetLapCounter: () => void;
    /** Push the start/finish gate [lat1,lon1,lat2,lon2] so the car counts laps itself (retained). */
    publishGate: (gate: [number, number, number, number]) => void;
    /** Push the lap-card layout the dash renders (retained: sent once, used until replaced). */
    publishLayout: (layout: unknown) => boolean;
    /** Push the park/pit-screen layout (retained, separate topic from the lap card). */
    publishParkLayout: (layout: unknown) => boolean;
    /** Push the live dynamic per-lap energy budget (Wh) the dash shows (retained). */
    publishLapBudget: (wh: number) => boolean;
    publishLapCardMs: (ms: number) => boolean;
    /** Push the active driver-message set the car holds (retained — survives dash reboot). */
    publishMessages: (messages: DashMessage[]) => boolean;
    /** Fire one driver message onto the dash now (not retained). durationS overrides the message's own. */
    sendMessage: (message: DashMessage, durationS?: number) => boolean;
    /** Dismiss whatever message is currently on the dash. */
    clearMessage: () => boolean;
    /** Latest mirror of the driver's screen (null until the car publishes state). */
    dashState: DashMirrorState | null;
    /** Wall-clock ms of the last lhre/dash/state message — for a link-silent warning. */
    lastStateAt: number | null;
    /** Last echoed control values, so trackside can confirm the car heard us. */
    acks: DashAcks;
}

function loadBrokerUrl(): string {
    const fallback = defaultBrokerUrl();
    if (typeof window === 'undefined') return fallback;
    const stored = localStorage.getItem(BROKER_STORAGE_KEY);
    // Drop a stale insecure ws:// override on an HTTPS page — it would just fail
    // as mixed content — and fall back to the secure same-origin default.
    if (stored && !(window.location.protocol === 'https:' && stored.startsWith('ws://'))) {
        return stored;
    }
    return fallback;
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

    // qos 0 = fire-and-forget (used for the targetPower keepalive — losing a
    // tick is fine, the next one corrects it). qos 1 = mqtt.js buffers the
    // publish in its outgoing store while disconnected and flushes on
    // reconnect, so a momentary link blip doesn't silently eat the message
    // (lapTrigger / lapReset can't tolerate a drop — see the call sites).
    const publish = useCallback((field: string, value: number, qos: 0 | 1 = 0) => {
        const client = clientRef.current;
        if (!client) return;
        client.publish(`${TOPIC_PREFIX}${field}`, String(value), { qos });
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
                    const state = JSON.parse(text) as DashMirrorState;
                    setDashState(state);
                    setLastStateAt(Date.now());
                    // Sync our local lap counter with the CAR'S authoritative
                    // lapCount. lapCounterRef resets to 0 on every page load,
                    // but dashd only fires the lap card on a rising-edge of
                    // lapTrigger vs the last value it saw. After a reload the
                    // website would send 1 while dashd still remembered N from
                    // the previous run -> 1 < N -> no rise -> no card. By
                    // matching the counter to the car's published lapCount the
                    // next sendLap is guaranteed to be a rising edge, and the
                    // two stay aligned across page reloads / leadership changes.
                    if (Number.isFinite(state.lapCount) && state.lapCount > lapCounterRef.current) {
                        lapCounterRef.current = state.lapCount;
                        setLapsSent(state.lapCount);
                    }
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

    const sendLap = useCallback((value?: number) => {
        // Caller passes an ABSOLUTE lap number (the website's authoritative
        // count). dashd adopts this directly so the dash displays the same
        // number as trackside. Falls back to incrementing the local counter
        // for backwards compat when no value is given.
        const desired = typeof value === 'number' && Number.isFinite(value) ? value : lapCounterRef.current + 1;
        // Keep the published value strictly rising so dashd's edge check passes
        // even when the caller's number happens to equal what we last sent.
        const n = Math.max(desired, lapCounterRef.current + 1);
        lapCounterRef.current = n;
        // qos 1: mqtt.js buffers the publish if the link is mid-reconnect and
        // flushes it on the next 'connect'. Without this a Tailscale blip at
        // the wrong moment silently swallowed the lap card.
        publish('lapTrigger', n, 1);
        setLapsSent(n);
    }, [publish]);

    // Belt-and-braces for the drain effect: re-publish a known lap value without
    // touching lapCounterRef. Use case: website thinks it sent N (so the local
    // counter is at N), but the car shows lapCount < N — either dashd missed the
    // publish entirely (restarted between the broker and dashd) or our publish
    // landed before dashd subscribed. Re-publishing the same value is safe
    // because dashd's edge check is forward-only.
    const republishLap = useCallback((value: number) => {
        if (!Number.isFinite(value) || value <= 0) return;
        publish('lapTrigger', value, 1);
    }, [publish]);

    // Tell dashd to drop its lap counter + baseline (new session on trackside).
    // Without this the FIRST few clicks of a fresh session would be silently
    // ignored on the car because dashd was still holding the prior session's
    // high water mark.
    const resetLapCounter = useCallback(() => {
        lapCounterRef.current = 0;
        setLapsSent(0);
        publish('lapReset', 1, 1);
    }, [publish]);

    const publishGate = useCallback((gate: [number, number, number, number]) => {
        const client = clientRef.current;
        if (!client || !client.connected) return;
        // Retained + QoS 1: the car re-loads the current gate on every reconnect,
        // and a freshly-booted dash gets it without the strategist re-sending.
        client.publish(`${TOPIC_PREFIX}sfGate`, JSON.stringify(gate), { qos: 1, retain: true });
    }, []);

    // Retained + QoS 1, exactly like sfGate: the dash re-loads the current layout
    // on every reconnect/reboot, so it's "send once, used until replaced".
    const publishLayout = useCallback((layout: unknown): boolean => {
        const client = clientRef.current;
        if (!client || !client.connected) return false;
        client.publish(`${TOPIC_PREFIX}layout`, JSON.stringify(layout), { qos: 1, retain: true });
        return true;
    }, []);

    // Park / pit-screen layout — same retained pattern, separate topic so the
    // two screens are authored independently.
    const publishParkLayout = useCallback((layout: unknown): boolean => {
        const client = clientRef.current;
        if (!client || !client.connected) return false;
        client.publish(`${TOPIC_PREFIX}parkLayout`, JSON.stringify(layout), { qos: 1, retain: true });
        return true;
    }, []);

    // Retained: the live per-lap Wh budget (remaining energy / remaining laps),
    // republished by trackside whenever it changes. The dash holds it and shows
    // the driver the current Wh/lap target.
    const publishLapBudget = useCallback((wh: number): boolean => {
        const client = clientRef.current;
        if (!client || !client.connected) return false;
        client.publish(`${TOPIC_PREFIX}lapBudgetWh`, String(wh), { qos: 1, retain: true });
        return true;
    }, []);

    // Retained: how long the on-car full-screen lap card stays up (ms). The dash
    // holds it and re-loads it on reconnect/reboot.
    const publishLapCardMs = useCallback((ms: number): boolean => {
        const client = clientRef.current;
        if (!client || !client.connected) return false;
        client.publish(`${TOPIC_PREFIX}lapCardMs`, String(Math.round(ms)), { qos: 1, retain: true });
        return true;
    }, []);

    // Retained, like sfGate/layout: the car re-loads the current quick-send set
    // on reconnect/reboot. This is the bounded set the dash holds.
    const publishMessages = useCallback((messages: DashMessage[]): boolean => {
        const client = clientRef.current;
        if (!client || !client.connected) return false;
        client.publish(`${TOPIC_PREFIX}messages`, JSON.stringify(messages), { qos: 1, retain: true });
        return true;
    }, []);

    // A one-shot trigger — NOT retained, so it fires once and doesn't replay on
    // reconnect. Carries the full message so a fresh dash can render it even
    // before the retained set arrives. `at` makes a repeat press a new event.
    const sendMessage = useCallback((message: DashMessage, durationS?: number): boolean => {
        const client = clientRef.current;
        if (!client || !client.connected) return false;
        const trigger: MessageTrigger = { at: Date.now(), durationS: durationS ?? message.durationS, msg: message };
        client.publish(`${TOPIC_PREFIX}message`, JSON.stringify(trigger), { qos: 0 });
        return true;
    }, []);

    const clearMessage = useCallback((): boolean => {
        const client = clientRef.current;
        if (!client || !client.connected) return false;
        const trigger: MessageTrigger = { at: Date.now(), clear: true };
        client.publish(`${TOPIC_PREFIX}message`, JSON.stringify(trigger), { qos: 0 });
        return true;
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
        republishLap,
        resetLapCounter,
        publishGate,
        publishLayout,
        publishParkLayout,
        publishLapBudget,
        publishLapCardMs,
        publishMessages,
        sendMessage,
        clearMessage,
        dashState,
        lastStateAt,
        acks,
    };
}
