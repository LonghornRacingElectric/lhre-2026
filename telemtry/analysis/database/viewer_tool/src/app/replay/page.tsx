"use client";

import React, { useEffect, useMemo, useRef, useState } from "react";
import LiveViewerBanner from "@/components/LiveViewerBanner";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { setSSEPath } from "@/lib/kafka/sseBus";
import { restartSSE } from "@/lib/kafka/sseBus";
import { stopSSE } from "@/lib/kafka/sseBus";

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
  const [speed, setSpeed] = useState(1);
  const [timeCursor, setTimeCursor] = useState<number | null>(null);

  const [streamCursorMs, setStreamCursorMs] = useState<number | null>(null);

  const [currentTimeMs, setCurrentTimeMs] = useState<number | null>(null);
  const [nextTimeMs, setNextTimeMs] = useState<number | null>(null);
  const [isEnd, setIsEnd] = useState(false);
  const lastFetchedMsRef = useRef<number | null>(null);
  const timeCursorRef = useRef<number | null>(null);

  useEffect(() => {
    timeCursorRef.current = timeCursor;
  }, [timeCursor]);

  const [dbCarData, setDbCarData] = useState<CarVisualizationData | null>(null);
  const [dbDriverData, setDbDriverData] = useState<DriverInputData | null>(null);

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
      setTimeCursor(null);
      setStreamCursorMs(null);
      setCurrentTimeMs(null);
      setNextTimeMs(null);
      setIsEnd(false);
      lastFetchedMsRef.current = null;
      setDbCarData(null);
      setDbDriverData(null);
      return;
    }

    (async () => {
      const res = await fetch(`/api/replay/summary?eventId=${selected.event_id}`);
      const json = (await res.json()) as ReplaySummary;
      setSummary(json);
      setTimeCursor(json.time_start);
      setStreamCursorMs(json.time_start);
      setCurrentTimeMs(null);
      setNextTimeMs(null);
      setIsEnd(false);
      lastFetchedMsRef.current = null;
      setPlaying(true);
    })();
  }, [selected]);

  useEffect(() => {
    if (!selected || timeCursor == null) {
      setDbCarData(null);
      setDbDriverData(null);
      setCurrentTimeMs(null);
      setNextTimeMs(null);
      setIsEnd(false);
      lastFetchedMsRef.current = null;
      return;
    }

    const cursorMs = Math.floor(timeCursor);
    if (lastFetchedMsRef.current != null && lastFetchedMsRef.current === cursorMs) {
      return;
    }

    const ac = new AbortController();
    (async () => {
      try {
        const res = await fetch(
          `/api/replay/state?eventId=${selected.event_id}&atTimeMs=${cursorMs}`,
          { signal: ac.signal }
        );
        if (!res.ok) return;
        const json = await res.json();
        const snappedMs = typeof json.time_ms === "number" && Number.isFinite(json.time_ms) ? json.time_ms : null;
        const nextMs = typeof json.next_time_ms === "number" && Number.isFinite(json.next_time_ms) ? json.next_time_ms : null;
        const end = Boolean(json.is_end);

        setDbCarData(json.car_visualization ?? null);
        setDbDriverData(json.driver_input_visualizer ?? null);

        if (snappedMs != null) {
          lastFetchedMsRef.current = snappedMs;
          setCurrentTimeMs(snappedMs);
          if (snappedMs !== cursorMs) setTimeCursor(snappedMs);
        } else {
          lastFetchedMsRef.current = cursorMs;
          setCurrentTimeMs(cursorMs);
        }

        setNextTimeMs(nextMs);
        setIsEnd(end);

        if (end) {
          setPlaying(false);
        }
      } catch {
        // ignore abort/network
      }
    })();

    return () => {
      ac.abort();
    };
  }, [selected, timeCursor]);

  const sseUrl = useMemo(() => {
    if (!selected) return "/api/replay-stream";
    const params = new URLSearchParams();
    params.set("eventId", String(selected.event_id));
    if (streamCursorMs != null) params.set("fromTimeMs", String(Math.floor(streamCursorMs)));
    params.set("speed", String(speed));
    if (!playing) params.set("paused", "1");
    return `/api/replay-stream?${params.toString()}`;
  }, [selected, streamCursorMs, speed, playing]);

  // If the user seeks, restart the replay stream from that time.
  // If playback is toggled, restart from the current cursor so pause shows the right snapshot.
  useEffect(() => {
    if (!selected) return;
    const cur = timeCursorRef.current;
    if (cur == null) return;
    setStreamCursorMs(cur);
  }, [selected, playing]);

  // Switch the global SSE source to replay stream while on this page
  useEffect(() => {
    if (!selected) {
      stopSSE();
      return () => {
        setSSEPath("/api/kafka-stream");
        restartSSE();
      };
    }
    setSSEPath(sseUrl);
    restartSSE();
    return () => {
      setSSEPath("/api/kafka-stream");
      restartSSE();
    };
  }, [selected, sseUrl]);

  const timeStart = summary ? toNum(summary.time_start) : null;
  const timeEnd = summary ? toNum(summary.time_end) : null;

  // Advance cursor when playing based on recorded packet time deltas.
  useEffect(() => {
    if (!selected) return;
    if (!playing) return;
    if (timeStart == null || timeEnd == null) return;
    if (currentTimeMs == null) return;

    if (currentTimeMs >= timeEnd) {
      setPlaying(false);
      return;
    }
    if (isEnd || nextTimeMs == null) {
      setPlaying(false);
      return;
    }

    const dt = Math.max(0, nextTimeMs - currentTimeMs);
    const waitMs = Math.max(0, Math.floor(dt / Math.max(0.1, speed)));

    const t = window.setTimeout(() => {
      setTimeCursor(nextTimeMs);
    }, waitMs);

    return () => window.clearTimeout(t);
  }, [selected, playing, speed, timeStart, timeEnd, currentTimeMs, nextTimeMs, isEnd]);

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
                      <Button onClick={() => setPlaying((p) => !p)}>
                        {playing ? "Pause" : "Play"}
                      </Button>
                      <div className="flex items-center gap-2">
                        <span className="text-sm text-gray-600">Speed</span>
                        <Input
                          type="number"
                          step={0.1}
                          min={0.1}
                          max={10}
                          value={speed}
                          onChange={(e) => setSpeed(Number(e.target.value) || 1)}
                          className="w-24"
                        />
                      </div>
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
                          setTimeCursor(next);
                          setStreamCursorMs(next);
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
