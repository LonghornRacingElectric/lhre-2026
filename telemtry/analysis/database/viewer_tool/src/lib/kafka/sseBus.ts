"use client";

// Lightweight SSE singleton with topic fan-out per page

export type KafkaEvent = {
  topic: string;
  partition: number;
  payload: string;
  headers?: Record<string, string | undefined>;
  offset: string;
  timestamp: string;
};

type Handler = (evt: KafkaEvent) => void;
const listeners = new Map<string, Set<Handler>>(); // exact topic keys

let es: EventSource | null = null;
let connected = false;
let lastMessageAt: number | undefined;
let ssePath = "/api/kafka-stream";
let currentUrl = "";

const connListeners = new Set<(c: boolean) => void>();

function buildTopicsParam(): string {
  const topics = Array.from(listeners.keys());
  return topics.join(",");
}

function buildUrl(): string {
  const u = new URL(
    ssePath,
    typeof window !== "undefined" ? window.location.origin : "http://localhost"
  );
  const topics = buildTopicsParam();
  if (topics) {
    u.searchParams.set("topics", topics);
  }
  return u.toString();
}

function matches(key: string, topic: string): boolean { return key === topic; }

function notifyConnection(state: boolean) {
  connected = state;
  for (const fn of connListeners) fn(state);
}

function openIfNeeded() {
  if (typeof window === "undefined") return;
  const url = buildUrl();
  if (es && currentUrl === url) return; // already correct
  // Recreate
  try {
    es?.close();
  } catch {}
  es = new EventSource(url);
  currentUrl = url;
  notifyConnection(false);

  es.onopen = () => notifyConnection(true);
  es.onerror = () => notifyConnection(false);
  es.onmessage = (e: MessageEvent) => {
    try {
      const evt: KafkaEvent = JSON.parse(e.data);
      if (!evt?.topic) return;
      lastMessageAt = Date.now();
      // Dispatch to any exact or prefix listeners
      for (const [key, set] of listeners) {
        if (matches(key, evt.topic)) {
          for (const fn of set) fn(evt);
        }
      }
    } catch {
      // ignore parse errors
    }
  };
}

export function subscribeSSE(topic: string, handler: Handler, opts?: { ssePath?: string }) {
  if (opts?.ssePath) ssePath = opts.ssePath;
  if (!listeners.has(topic)) listeners.set(topic, new Set());
  listeners.get(topic)!.add(handler);
  openIfNeeded();
  return () => {
    const set = listeners.get(topic);
    if (!set) return;
    set.delete(handler);
    if (set.size === 0) listeners.delete(topic);
    // Reopen with new topic set if changed
    openIfNeeded();
  };
}

export function onConnectionChange(cb: (connected: boolean) => void) {
  connListeners.add(cb);
  return () => connListeners.delete(cb);
}

export function getSSEState() {
  return { connected, lastMessageAt };
}

export function restartSSE() {
  if (!es) return;
  try { es.close(); } catch {}
  es = null;
  openIfNeeded();
}
