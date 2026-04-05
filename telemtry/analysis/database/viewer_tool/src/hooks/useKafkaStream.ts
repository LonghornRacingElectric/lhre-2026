"use client";

import { useEffect, useMemo, useRef, useState } from "react";
import { getSSEPath, getSSEState, onConnectionChange, restartSSE, subscribeSSE } from "@/lib/kafka/sseBus";

export type KafkaEvent = {
  topic: string;
  partition: number;
  payload: string; // raw payload string from Kafka
  headers?: Record<string, string | undefined>;
  offset: string;
  timestamp: string;
};

export type UseKafkaStreamOptions<TData = unknown, TSelected = TData> = {
  topic: string;            // e.g., "telemetry" or "telemetry/*"
  prefix?: boolean;         // treat topic as a prefix (also implied by trailing /*)
  parse?: (payload: string) => TData; // defaults to JSON.parse fallback to string
  select?: (data: TData, evt: KafkaEvent) => TSelected; // pick specific data from message
  filter?: (evt: KafkaEvent) => boolean; // additional filter besides topic
  initial?: TSelected | (() => TSelected);
  onMessage?: (evt: KafkaEvent, data: TData, selected: TSelected) => void; // side-effects
  onError?: (err: Event) => void;
  ssePath?: string;         // override SSE route, default "/api/kafka-stream"
  staleAfterMs?: number;    // consider data flow stale if no message for this duration (default 5000)
  merge?: boolean;          // if true, shallow merge new data with existing data (reset if new data is empty object)
};

export type UseKafkaStreamState<TSelected> = {
  data: TSelected | undefined;
  lastEvent?: KafkaEvent;
  connected: boolean;        // SSE transport connected/open
  kafkaConnected: boolean;   // Receiving fresh messages within staleAfterMs window
  lastMessageAt?: number;    // timestamp (ms) when last message arrived
  error?: string;
  restart: () => void;
  close: () => void;
};

function defaultParse(payload: string): any {
  try {
    return JSON.parse(payload);
  } catch {
    return payload;
  }
}

function isObject(item: any) {
  return (item && typeof item === 'object' && !Array.isArray(item));
}

function deepMerge(target: any, source: any): any {
  if (isObject(target) && isObject(source)) {
    const output = { ...target };
    Object.keys(source).forEach(key => {
      if (isObject(source[key])) {
        if (!(key in target)) {
          Object.assign(output, { [key]: source[key] });
        } else {
          output[key] = deepMerge(target[key], source[key]);
        }
      } else {
        Object.assign(output, { [key]: source[key] });
      }
    });
    return output;
  }
  return source;
}

/**
 * Subscribe to Kafka messages via the SSE route and derive specific data using `select`.
 * Example:
 *   const speed = useKafkaStream<number>({
 *     topic: 'telemetry/*',
 *     select: (data) => data?.vehicle?.speed ?? 0
 *   });
 */
export function useKafkaStream<TData = unknown, TSelected = TData>(
  opts: UseKafkaStreamOptions<TData, TSelected>
): UseKafkaStreamState<TSelected> {
  const {
    topic,
    prefix,
    parse = defaultParse,
    select,
    filter,
    initial,
    onMessage,
    onError,
    ssePath = getSSEPath(),
    staleAfterMs = 100,
    merge = false,
  } = opts;

  const [connected, setConnected] = useState(false); // SSE socket state
  const [error, setError] = useState<string | undefined>(undefined);
  const [lastEvent, setLastEvent] = useState<KafkaEvent | undefined>(undefined);
  const [data, setData] = useState<TSelected | undefined>(() =>
    typeof initial === "function" ? (initial as any)() : initial
  );
  const [lastMessageAt, setLastMessageAt] = useState<number | undefined>(undefined);
  const [kafkaConnected, setKafkaConnected] = useState<boolean>(false); // message freshness state

  const restartRef = useRef<() => void>(() => {});

  const { url, isPrefix } = useMemo(() => {
    const hasWildcard = topic.endsWith("/*");
    const base = hasWildcard ? topic.slice(0, -2) : topic;
    const usePrefix = !!prefix || hasWildcard;
    const u = new URL(
      ssePath,
      typeof window !== "undefined" ? window.location.origin : "http://localhost"
    );
    u.searchParams.set("topic", base + (hasWildcard ? "/*" : ""));
    if (!hasWildcard && usePrefix) u.searchParams.set("prefix", "true");
    return { url: u.toString(), isPrefix: usePrefix };
  }, [topic, prefix, ssePath]);

  useEffect(() => {
    if (typeof window === "undefined") return;
    // track SSE connection state from the singleton bus
    const offConn = onConnectionChange((c) => setConnected(c));
    // set initial state
    const st = getSSEState();
    setConnected(st.connected);
    if (st.lastMessageAt) setLastMessageAt(st.lastMessageAt);

    const unsub = subscribeSSE(topic, (evt) => {
      try {
        if (filter && !filter(evt)) return;
        const parsed: TData = parse(evt.payload);
        const selected: TSelected = (select ? select(parsed, evt) : (parsed as unknown as TSelected));
        setLastEvent(evt);
        
        if (merge) {
          setData((prev) => {
            // If selected is an empty object, treat as reset
            if (selected && typeof selected === "object" && !Array.isArray(selected) && Object.keys(selected).length === 0) {
              return selected;
            }
            // If both are objects, deep merge
            if (prev && typeof prev === "object" && !Array.isArray(prev) && 
                selected && typeof selected === "object" && !Array.isArray(selected)) {
              return deepMerge(prev, selected);
            }
            return selected;
          });
        } else {
          setData(selected);
        }

        const now = Date.now();
        setLastMessageAt(now);
        setKafkaConnected(true);
        onMessage?.(evt, parsed, selected);
      } catch (err) {
        setError(err instanceof Error ? err.message : String(err));
      }
    }, { ssePath });

    restartRef.current = () => restartSSE();
    return () => { offConn(); unsub(); };
  }, [topic, ssePath, filter, parse, select, onMessage, merge]);

  // Freshness monitoring: mark kafkaConnected false if stale
  useEffect(() => {
    const poll = () => {
      if (!lastMessageAt) {
        setKafkaConnected(false);
        return;
      }
      const age = Date.now() - lastMessageAt;
      if (age > staleAfterMs) {
        if (kafkaConnected) setKafkaConnected(false);
        // Clear stale data so UI doesn't display outdated values that look "live"
        setData(undefined);
        setLastEvent(undefined);
      } else if (!kafkaConnected) {
        setKafkaConnected(true);
      }
    };
    const interval = setInterval(poll, Math.min(1000, Math.max(250, staleAfterMs / 2)));
    return () => interval && clearInterval(interval);
  }, [lastMessageAt, staleAfterMs, kafkaConnected]);

  // Reset data when topic changes to avoid showing previous topic's last payload
  useEffect(() => {
    setData(typeof initial === "function" ? (initial as any)() : initial);
    setLastEvent(undefined);
    setLastMessageAt(undefined);
    setKafkaConnected(false);
  }, [topic, initial]);

  return {
    data,
    lastEvent,
    connected,
    kafkaConnected,
    lastMessageAt,
    error,
    restart: () => restartRef.current?.(),
    // With singleton SSE we just trigger a restart (no direct close to individual ES)
    close: () => { restartSSE(); },
  };
}

/** Convenience helper for JSON payloads with a simple selector. */
export function useKafkaJSON<TSelected = unknown>(opts: Omit<UseKafkaStreamOptions<any, TSelected>, "parse">) {
  return useKafkaStream<any, TSelected>({ ...opts, parse: defaultParse, staleAfterMs: 100 });
}