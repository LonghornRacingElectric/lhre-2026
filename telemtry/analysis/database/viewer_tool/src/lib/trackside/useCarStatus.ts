"use client";

import { useEffect, useRef, useState } from "react";

/**
 * Real car state from the central classifier (telemetry/car-status, PR #282),
 * surfaced over `GET /api/car-status/stream?car=<orion|angelique>` (SSE).
 *
 * This is deliberately decoupled from "is telemetry arriving" — the classifier
 * knows whether the car is physically OFF / idling / ready-to-drive / moving,
 * which is what the dashboard should trust instead of optimistically showing
 * "ready" whenever any packet flows.
 *
 * The route ships on the car-status branch and may not exist yet in every
 * deployment, so this hook degrades gracefully: a bounded number of reconnect
 * attempts, then it goes quiet and reports `available: false` (no console spam,
 * no infinite reconnect loop).
 */

export type CarState = "OFF" | "ON_IDLE" | "READY" | "MOVING";

export type CarStatusEvent = {
  car: string;
  kind: "transition" | "heartbeat";
  state: CarState;
  reasons: string[];
  active_faults: string[];
  time_in_state_ms: number;
  hv_soc: number | null;
  hv_pack_v: number | null;
  lv_v: number | null;
  lv_c: number | null;
  lv_t: number | null;
  thresholds?: Record<string, number>;
  t_ms: number;
};

export type CarStatusFeed = {
  /** Latest event received, or null before the first event. */
  event: CarStatusEvent | null;
  /** Convenience: latest classifier state, or null if none yet. */
  state: CarState | null;
  /** Advisory faults from the latest non-heartbeat event (never folded into state). */
  faults: string[];
  /** True once we've connected and received at least one event. */
  available: boolean;
  /** Wall-clock ms when the last event arrived (for staleness checks). */
  lastEventAt: number | null;
};

const MAX_ATTEMPTS = 4;

const EMPTY: CarStatusFeed = { event: null, state: null, faults: [], available: false, lastEventAt: null };

export function useCarStatus(car: string | null, enabled = true): CarStatusFeed {
  const [feed, setFeed] = useState<CarStatusFeed>(EMPTY);
  const sourceRef = useRef<EventSource | null>(null);
  const retryRef = useRef<number | null>(null);

  useEffect(() => {
    if (!enabled || !car || typeof window === "undefined") {
      setFeed(EMPTY);
      return;
    }

    let cancelled = false;
    let attempts = 0;
    setFeed(EMPTY);

    const cleanup = () => {
      sourceRef.current?.close();
      sourceRef.current = null;
      if (retryRef.current != null) {
        window.clearTimeout(retryRef.current);
        retryRef.current = null;
      }
    };

    const connect = () => {
      if (cancelled) return;
      const es = new EventSource(`/api/car-status/stream?car=${encodeURIComponent(car)}`);
      sourceRef.current = es;

      es.onmessage = (msg) => {
        attempts = 0; // a working stream resets the retry budget
        try {
          const parsed = JSON.parse(msg.data) as Partial<CarStatusEvent>;
          if (!parsed || typeof parsed.state !== "string") return;
          const next = parsed as CarStatusEvent;
          setFeed((prev) => ({
            event: next,
            state: next.state,
            // Heartbeats carry no fresh fault scan — keep the last known set.
            faults: next.kind === "heartbeat" ? prev.faults : next.active_faults ?? [],
            available: true,
            lastEventAt: Date.now(),
          }));
        } catch {
          // ignore malformed frames
        }
      };

      es.onerror = () => {
        es.close();
        sourceRef.current = null;
        if (cancelled) return;
        attempts += 1;
        if (attempts >= MAX_ATTEMPTS) {
          // Endpoint absent or persistently failing — go quiet.
          setFeed((prev) => ({ ...prev, available: false }));
          return;
        }
        retryRef.current = window.setTimeout(connect, Math.min(8000, 1000 * 2 ** (attempts - 1)));
      };
    };

    connect();
    return () => {
      cancelled = true;
      cleanup();
    };
  }, [car, enabled]);

  return feed;
}

/** Display metadata for each classifier state. */
export const CAR_STATE_META: Record<CarState, { label: string; cls: string }> = {
  OFF: { label: "Off", cls: "stateOff" },
  ON_IDLE: { label: "On · Idle", cls: "stateIdle" },
  READY: { label: "Ready to Drive", cls: "stateReady" },
  MOVING: { label: "Moving", cls: "stateMoving" },
};

export function humanizeReason(reason: string): string {
  return reason.replace(/_/g, " ").replace(/\b\w/g, (c) => c.toUpperCase());
}
