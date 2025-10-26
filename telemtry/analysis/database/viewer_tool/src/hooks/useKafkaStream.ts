"use client";

import { useEffect, useMemo, useRef, useState } from "react";

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
};

export type UseKafkaStreamState<TSelected> = {
  data: TSelected | undefined;
  lastEvent?: KafkaEvent;
  connected: boolean;
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
    ssePath = "/api/kafka-stream",
  } = opts;

  const [connected, setConnected] = useState(false);
  const [error, setError] = useState<string | undefined>(undefined);
  const [lastEvent, setLastEvent] = useState<KafkaEvent | undefined>(undefined);
  const [data, setData] = useState<TSelected | undefined>(() =>
    typeof initial === "function" ? (initial as any)() : initial
  );

  const esRef = useRef<EventSource | null>(null);
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
    // Ensure we only create EventSource in the browser
    if (typeof window === "undefined") return;

    const es = new EventSource(url);
    esRef.current = es;
    setConnected(false);
    setError(undefined);

    es.onopen = () => setConnected(true);
    es.onerror = (evt) => {
      setConnected(false);
      setError("stream error");
      onError?.(evt);
    };

    es.onmessage = (e) => {
      try {
        const evt: KafkaEvent = JSON.parse(e.data);
        if (filter && !filter(evt)) return;
        const parsed: TData = parse(evt.payload);
        const selected: TSelected = (select ? select(parsed, evt) : (parsed as unknown as TSelected));
        setLastEvent(evt);
        setData(selected);
        onMessage?.(evt, parsed, selected);
      } catch (err) {
        setError(err instanceof Error ? err.message : String(err));
      }
    };

    const close = () => {
      try { es.close(); } catch {}
      esRef.current = null;
      setConnected(false);
    };

    restartRef.current = () => {
      close();
      // Trigger effect to recreate EventSource by changing dependency; simplest approach is to rely on url changes.
      // If url didn't change, we just re-open here:
      const es2 = new EventSource(url);
      esRef.current = es2;
      es2.onopen = () => setConnected(true);
      es2.onerror = (evt) => { setConnected(false); setError("stream error"); onError?.(evt); };
      es2.onmessage = es.onmessage;
    };

    return close;
  }, [url, filter, parse, select, onMessage, onError]);

  return {
    data,
    lastEvent,
    connected,
    error,
    restart: () => restartRef.current?.(),
    close: () => { try { esRef.current?.close(); } finally { esRef.current = null; setConnected(false); } },
  };
}

/** Convenience helper for JSON payloads with a simple selector. */
export function useKafkaJSON<TSelected = unknown>(opts: Omit<UseKafkaStreamOptions<any, TSelected>, "parse">) {
  return useKafkaStream<any, TSelected>({ ...opts, parse: defaultParse });
}
