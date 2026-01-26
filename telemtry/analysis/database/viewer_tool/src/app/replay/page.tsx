"use client";

import React, { useEffect, useMemo, useRef, useState } from "react";
import LiveViewerBanner from "@/components/LiveViewerBanner";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";

// Reuse the same widgets as Live Viewer
import CarVisualization from "@/components/CarVisualization";
import DriverInputVisualizer from "@/components/DriverInputVisualizer";
import type { CarVisualizationData } from "@/components/CarVisualization";
import type { DriverInputData } from "@/components/DriverInputVisualizer";

type ReplayEvent = {
  event_id: number;
  status: number | null;
  creation_time: string | number;
  start_time: string | number | null;
  end_time: string | number | null;
  packet_start: string | number | null;
  packet_end: string | number | null;
  day_date: string | null;
  driver_name: string | null;
  area: string | null;
  track: string | null;
  event_type: string | null;
  car_name: string | null;
};

type ReplaySummary = {
  event_id: number;
  packet_start: string | null;
  packet_end: string | null;
  time_start: number | null;
  time_end: number | null;
  time_start_raw?: string | null;
  time_end_raw?: string | null;
  time_scale_to_ms?: string | null;
  lap_times: Array<{ start_time: string; end_time: string | null; notes: string | null }>;
  flagged_events: Array<{ type: string; start_time: string; end_time: string | null; notes: string | null }>;
};

type ReplayStatePayload = {
  cursor_ms: number | null;
  time_ms: number | null;
  packet_id: string;
  is_end: boolean;
  car_visualization: CarVisualizationData | null;
  driver_input_visualizer: DriverInputData | null;
};

function toNum(v: any): number | null {
  if (v == null) return null;
  const n = Number(v);
  return Number.isFinite(n) ? n : null;
}

export default function ReplayPage() {
  const [events, setEvents] = useState<ReplayEvent[]>([]);
  const [selected, setSelected] = useState<ReplayEvent | null>(null);
  const [summary, setSummary] = useState<ReplaySummary | null>(null);

  const [playing, setPlaying] = useState(true);
  const [isEnd, setIsEnd] = useState(false);
  const [timeCursor, setTimeCursor] = useState<number | null>(null);

  // Anchor point for (re)connecting the SSE stream.
  // Do NOT update this from incoming stream data.
  const [streamStartAtMs, setStreamStartAtMs] = useState<number | null>(null);


  const [dbCarData, setDbCarData] = useState<CarVisualizationData | null>(null);
  const [dbDriverData, setDbDriverData] = useState<DriverInputData | null>(null);

  const esRef = useRef<EventSource | null>(null);

  // Load events list
  useEffect(() => {
    (async () => {
      const res = await fetch("/api/replay/events");
      const json = await res.json();
      setEvents(json.events || []);
    })();
  }, []);

  // When selecting an event, initialize cursor
  useEffect(() => {
    if (!selected) {
      setSummary(null);
      setIsEnd(false);
      setDbCarData(null);
      setDbDriverData(null);
      setTimeCursor(null);
      setStreamStartAtMs(null);
      return;
    }

    (async () => {
      const res = await fetch(`/api/replay/summary?eventId=${selected.event_id}`);
      const json = (await res.json()) as ReplaySummary;
      setSummary(json);
      setIsEnd(false);
      setPlaying(true);

      const start = toNum(json.time_start);
      setTimeCursor(start);
      setStreamStartAtMs(start);
    })();
  }, [selected]);

  const timeStart = summary ? toNum(summary.time_start) : null;
  const timeEnd = summary ? toNum(summary.time_end) : null;

  // Stream replay state over SSE.
  useEffect(() => {
    if (esRef.current) {
      esRef.current.close();
      esRef.current = null;
    }

    if (!selected?.event_id) return;
    if (streamStartAtMs == null) return;
    // Once we've reached the end, do not reconnect (it would snap back to the old anchor).
    // Seeking sets isEnd=false, which allows reconnecting again.
    if (isEnd) return;

    const tickMs = 100;
    const url = `/api/replay/stream?eventId=${selected.event_id}` +
      `&startAtTimeMs=${streamStartAtMs}` +
      `&playing=${playing ? "1" : "0"}` +
      `&tickMs=${tickMs}`;

    const es = new EventSource(url);
    esRef.current = es;

    let gotFirst = false;

    const onState = (ev: MessageEvent) => {
      gotFirst = true;

      let payload: ReplayStatePayload | null = null;
      try {
        payload = JSON.parse(ev.data);
      } catch {
        return;
      }
      if (!payload) return;

      setDbCarData(payload.car_visualization ?? null);
      setDbDriverData(payload.driver_input_visualizer ?? null);

      const end = Boolean(payload.is_end);
      setIsEnd(end);

      if (end) {
        setPlaying(false);
        const nextCursor = payload.cursor_ms ?? payload.time_ms ?? null;
        setTimeCursor(timeEnd ?? nextCursor);
        es.close();
        if (esRef.current === es) esRef.current = null;
        return;
      }

      const nextCursor = payload.cursor_ms ?? payload.time_ms ?? null;
      if (nextCursor != null) setTimeCursor(nextCursor);

      // If paused, treat the SSE request as a one-shot snapshot.
      if (!playing) {
        es.close();
        if (esRef.current === es) esRef.current = null;
      }
    };

    const onError = () => {
      // When paused the server closes quickly; do not keep a reconnect loop alive.
      if (!playing) {
        es.close();
        if (esRef.current === es) esRef.current = null;
      }
    };

    es.addEventListener("state", onState as any);
    es.onerror = onError;

    // Safety: if paused and the server never sends, stop the connection.
    if (!playing) {
      const t = window.setTimeout(() => {
        if (!gotFirst && esRef.current === es) {
          es.close();
          esRef.current = null;
        }
      }, 1500);

      return () => {
        window.clearTimeout(t);
        es.removeEventListener("state", onState as any);
        es.close();
        if (esRef.current === es) esRef.current = null;
      };
    }

    return () => {
      es.removeEventListener("state", onState as any);
      es.close();
      if (esRef.current === es) esRef.current = null;
    };
  }, [selected?.event_id, playing, streamStartAtMs, isEnd, timeEnd]);

  const markers = useMemo(() => {
    if (!summary || timeStart == null || timeEnd == null || timeEnd <= timeStart) return [];

    const out: Array<{ kind: "lap" | "flag"; t: number; label: string }> = [];
    for (const l of summary.lap_times || []) {
      const t = Number(l.start_time);
      if (!Number.isFinite(t)) continue;
      out.push({ kind: "lap", t, label: l.notes ?? "Lap" });
    }
    for (const f of summary.flagged_events || []) {
      const t = Number(f.start_time);
      if (!Number.isFinite(t)) continue;
      out.push({ kind: "flag", t, label: f.type });
    }
    return out;
  }, [summary, timeStart, timeEnd]);

  const handleTogglePlay = () => {
    setPlaying((p) => {
      const next = !p;
      if (next) {
        const anchor = timeCursor ?? timeStart;
        if (anchor != null) setStreamStartAtMs(anchor);
      }
      return next;
    });
  };

  return (
    <>
      <LiveViewerBanner />
      <div className="container mx-auto p-8 pt-24 md:pt-20">
        <div className="grid grid-cols-1 lg:grid-cols-[360px_1fr] gap-8">
          <Card className="h-[calc(100vh-140px)] overflow-hidden">
            <CardHeader>
              <CardTitle>Replay Events</CardTitle>
            </CardHeader>
            <CardContent className="h-full overflow-auto space-y-2">
              {events.map((e) => {
                const isSel = selected?.event_id === e.event_id;
                return (
                  <button
                    key={e.event_id}
                    className={`w-full text-left border rounded p-2 ${isSel ? "bg-gray-100" : "bg-white"}`}
                    onClick={() => setSelected(e)}
                  >
                    <div className="font-semibold">Event #{e.event_id}</div>
                    <div className="text-xs text-gray-600">
                      {e.car_name ?? "—"} | {e.driver_name ?? "—"} | {e.track ?? "—"}
                    </div>
                    <div className="text-xs text-gray-500">{e.event_type ?? "—"}</div>
                  </button>
                );
              })}
            </CardContent>
          </Card>

          <div className="space-y-6">
            {selected && summary ? (
              <Card>
                <CardHeader>
                  <CardTitle>Summary</CardTitle>
                </CardHeader>
                <CardContent className="space-y-4">
                  <div>
                    <div className="text-sm font-medium">Lap times</div>
                    {summary.lap_times?.length ? (
                      <div className="mt-2 space-y-1">
                        {summary.lap_times.map((l, idx) => (
                          <div key={`${l.start_time}-${idx}`} className="text-sm text-gray-700">
                            {new Date(Number(l.start_time)).toLocaleTimeString()} {l.notes ? `— ${l.notes}` : ""}
                          </div>
                        ))}
                      </div>
                    ) : (
                      <div className="text-sm text-gray-500 mt-1">None</div>
                    )}
                  </div>

                  <div>
                    <div className="text-sm font-medium">Flagged events</div>
                    {summary.flagged_events?.length ? (
                      <div className="mt-2 space-y-1">
                        {summary.flagged_events.map((f, idx) => (
                          <div key={`${f.type}-${f.start_time}-${idx}`} className="text-sm text-gray-700">
                            {new Date(Number(f.start_time)).toLocaleTimeString()} — {f.type}{f.notes ? ` (${f.notes})` : ""}
                          </div>
                        ))}
                      </div>
                    ) : (
                      <div className="text-sm text-gray-500 mt-1">None</div>
                    )}
                  </div>
                </CardContent>
              </Card>
            ) : null}

            <Card>
              <CardHeader>
                <CardTitle>Replay Controls</CardTitle>
              </CardHeader>
              <CardContent className="flex flex-col gap-4">
                {!selected ? (
                  <div className="text-gray-600">Select an event to start replay.</div>
                ) : (
                  <>
                    <div className="flex items-center gap-3 flex-wrap">
                      <Button onClick={handleTogglePlay} disabled={isEnd}>
                        {playing ? "Pause" : "Play"}
                      </Button>
                    </div>

                    <div className="flex flex-col gap-2">
                      <div className="text-sm text-gray-600">Timeline</div>

                      {timeStart != null && timeEnd != null && timeEnd > timeStart ? (
                        <div className="relative h-6">
                          {markers.map((m, idx) => {
                            const pct = ((m.t - timeStart) / (timeEnd - timeStart)) * 100;
                            if (!Number.isFinite(pct)) return null;
                            return (
                              <div
                                key={`${m.kind}-${m.t}-${idx}`}
                                className="absolute top-0 -translate-x-1/2 text-xs select-none"
                                style={{ left: `${Math.max(0, Math.min(100, pct))}%` }}
                                title={m.label}
                              >
                                {m.kind === "lap" ? "⏱" : "⚑"}
                              </div>
                            );
                          })}
                        </div>
                      ) : null}
                      <input
                        type="range"
                        min={timeStart ?? 0}
                        max={timeEnd ?? 0}
                        value={timeCursor ?? timeStart ?? 0}
                        onChange={(e) => {
                          const v = Number(e.target.value);
                          const next = Number.isFinite(v) ? v : timeStart;

                          setTimeCursor(next ?? null);
                          setIsEnd(false);
                          if (next != null) setStreamStartAtMs(next);
                        }}
                        disabled={timeStart == null || timeEnd == null}
                      />
                      <div className="text-xs text-gray-500">
                        {timeCursor != null ? new Date(timeCursor).toLocaleTimeString() : "—"}
                        {" "}/{" "}
                        {timeEnd != null ? new Date(timeEnd).toLocaleTimeString() : "—"}
                      </div>
                    </div>
                  </>
                )}
              </CardContent>
            </Card>

            {selected ? (
              <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-8 grid-auto-rows-[400px]">
                <Card className="h-full flex flex-col">
                  <CardHeader><CardTitle>3D Simulation</CardTitle></CardHeader>
                  <CardContent className="flex-grow"><CarVisualization data={dbCarData} /></CardContent>
                </Card>
                <Card className="h-full flex flex-col">
                  <CardHeader><CardTitle>Driver Input Visualizer</CardTitle></CardHeader>
                  <CardContent className="flex-grow"><DriverInputVisualizer data={dbDriverData} /></CardContent>
                </Card>
              </div>
            ) : null}
          </div>
        </div>
      </div>
    </>
  );
}
