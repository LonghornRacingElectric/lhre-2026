"use client";

import React, { useEffect, useMemo, useRef, useState } from "react";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";

// Reuse the same widgets as Live Viewer
import CarVisualization from "@/components/CarVisualization";
import DriverInputVisualizer from "@/components/DriverInputVisualizer";
import type { CarVisualizationData } from "@/components/CarVisualization";
import type { DriverInputData } from "@/components/DriverInputVisualizer";
import dynamic from "next/dynamic";
import { MapData } from "@/components/Map";
import Banner from "@/components/Banner";

const DynamicMap = dynamic(() => import("@/components/Map"), {
  ssr: false,
});

function TimelineLapIcon(props: { className?: string }) {
  return (
    <svg
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="2"
      strokeLinecap="round"
      strokeLinejoin="round"
      className={props.className}
      aria-hidden="true"
    >
      <circle cx="12" cy="13" r="8" />
      <path d="M12 13l3-2" />
      <path d="M12 5V3" />
      <path d="M8 3h8" />
    </svg>
  );
}

function TimelineFlagIcon(props: { className?: string }) {
  return (
    <svg
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="2"
      strokeLinecap="round"
      strokeLinejoin="round"
      className={props.className}
      aria-hidden="true"
    >
      <path d="M4 22V2" />
      <path d="M4 4h12l-2 4 2 4H4" />
    </svg>
  );
}

type ReplayEvent = {
  event_id: number;
  day_id?: number | null;
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

type DriveDay = {
  day_id: number;
  date: string;
  power_limit: number | null;
  air_temperature: number | null;
  relative_humidity: number | null;
  track_temperature: number | null;
};

type DriveDayGroup = {
  drive_day: DriveDay;
  event_count: number;
};

type ReplaySummary = {
  event_id: number;
  day_id?: number | null;
  day_date?: string | null;
  driver_name?: string | null;
  area?: string | null;
  track?: string | null;
  event_type?: string | null;
  car_name?: string | null;
  packet_start: string | null;
  packet_end: string | null;
  time_start: number | null;
  time_end: number | null;
  time_start_raw?: string | null;
  time_end_raw?: string | null;
  time_scale_to_ms?: string | null;
  lap_times: Array<{ start_time: string; end_time: string | null; notes: string | null }>;
  flagged_events: Array<{ type: string; start_time: string; end_time: string | null; notes: string | null }>;
  event_details?: Record<string, string | number | boolean>;
};

type ReplayStatePayload = {
  cursor_ms: number | null;
  time_ms: number | null;
  packet_id: string;
  is_end: boolean;
  car_visualization: CarVisualizationData | null;
  map_data: MapData | null;
  driver_input_visualizer: DriverInputData | null;
};

function toNum(v: any): number | null {
  if (v == null) return null;
  const n = Number(v);
  return Number.isFinite(n) ? n : null;
}

export default function ReplayPage() {
  const [driveDays, setDriveDays] = useState<DriveDayGroup[]>([]);
  const [selectedDay, setSelectedDay] = useState<DriveDayGroup | null>(null);
  const [selectedDayEvents, setSelectedDayEvents] = useState<ReplayEvent[]>([]);
  const [eventsLoading, setEventsLoading] = useState(false);
  const [selected, setSelected] = useState<ReplayEvent | null>(null);
  const [summary, setSummary] = useState<ReplaySummary | null>(null);

  const [playing, setPlaying] = useState(false);
  const [isEnd, setIsEnd] = useState(false);
  const [timeCursor, setTimeCursor] = useState<number | null>(null);
  const timeCursorRef = useRef<number | null>(null);

  // Anchor point for (re)connecting the SSE stream.
  // Do NOT update this from incoming stream data.
  const [streamStartAtMs, setStreamStartAtMs] = useState<number | null>(null);


  const [dbCarData, setDbCarData] = useState<CarVisualizationData | null>(null);
  const [dbDriverData, setDbDriverData] = useState<DriverInputData | null>(null);
  const [dbMapData, setDbMapData] = useState<MapData | null>(null);

  const esRef = useRef<EventSource | null>(null);

  // Load drive days list
  useEffect(() => {
    (async () => {
      const res = await fetch("/api/replay/drive-days");
      const json = await res.json();
      const days: DriveDayGroup[] = json.drive_days || [];
      setDriveDays(days);
      setSelectedDay((prev) => prev ?? (days.length ? days[0] : null));
    })();
  }, []);

  // When selecting a drive day, clear selected event and playback state
  useEffect(() => {
    setSelected(null);
    setSummary(null);
    setIsEnd(false);
    setPlaying(false);
    setDbCarData(null);
    setDbDriverData(null);
    setDbMapData(null);
    setTimeCursor(null);
    timeCursorRef.current = null;
    setStreamStartAtMs(null);

    if (!selectedDay?.drive_day?.day_id) {
      setSelectedDayEvents([]);
      setEventsLoading(false);
      return;
    }

    let cancelled = false;
    setEventsLoading(true);
    setSelectedDayEvents([]);

    (async () => {
      try {
        const res = await fetch(
          `/api/replay/drive-days/${selectedDay.drive_day.day_id}/events`,
        );
        const json = await res.json();
        if (cancelled) return;
        setSelectedDayEvents(json.events || []);
      } finally {
        if (!cancelled) setEventsLoading(false);
      }
    })();

    return () => {
      cancelled = true;
    };
  }, [selectedDay?.drive_day.day_id]);

  // When selecting an event, initialize cursor
  useEffect(() => {
    if (!selected) {
      setSummary(null);
      setIsEnd(false);
      setDbCarData(null);
      setDbDriverData(null);
      setTimeCursor(null);
      timeCursorRef.current = null;
      setStreamStartAtMs(null);
      return;
    }

    (async () => {
      const selectedCar =
        selected.car_name && selected.car_name !== "—"
          ? selected.car_name.toLowerCase()
          : "";
      const summaryUrl = selectedCar
        ? `/api/replay/summary?eventId=${selected.event_id}&car=${encodeURIComponent(selectedCar)}`
        : `/api/replay/summary?eventId=${selected.event_id}`;
      const res = await fetch(summaryUrl);
      const json = (await res.json()) as ReplaySummary;
      setSummary(json);
      setIsEnd(false);
      setPlaying(false);

      const start = toNum(json.time_start);
      setTimeCursor(start);
      timeCursorRef.current = start;
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

    const tickMs = 100; // modify this to increase the data precision
    const selectedCar =
      selected.car_name && selected.car_name !== "—"
        ? selected.car_name.toLowerCase()
        : "";
    const url = `/api/replay/stream?eventId=${selected.event_id}` +
      `&startAtTimeMs=${streamStartAtMs}` +
      `&playing=${playing ? "1" : "0"}` +
      `&tickMs=${tickMs}` +
      (selectedCar ? `&car=${encodeURIComponent(selectedCar)}` : "");

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
      setDbMapData(payload.map_data ?? null);

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
      timeCursorRef.current = nextCursor;

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
  }, [selected?.event_id, selected?.car_name, playing, streamStartAtMs, isEnd, timeEnd]);

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
      const anchor = timeCursorRef.current ?? timeCursor ?? timeStart;
      // On both play and pause, anchor the stream to the current cursor.
      // This prevents "pause" from snapping back to the original stream start.
      if (anchor != null) setStreamStartAtMs(anchor);
      return next;
    });
  };

  const seekTo = (ms: number) => {
    if (!Number.isFinite(ms)) return;
    setTimeCursor(ms);
    timeCursorRef.current = ms;
    setIsEnd(false);
    setStreamStartAtMs(ms);
  };

  const handleRestart = () => {
    if (timeStart == null) return;
    // Reset playhead to the event start; keep paused so user can hit Play.
    setPlaying(true);
    seekTo(timeStart);
  };

  const selectedDayInfo = selectedDay?.drive_day ?? null;

  const eventDescription = (() => {
    const car = selected?.car_name ?? summary?.car_name;
    const driver = selected?.driver_name ?? summary?.driver_name;
    const track = selected?.track ?? summary?.track;
    const area = selected?.area ?? summary?.area;
    const type = selected?.event_type ?? summary?.event_type;
    const pieces = [car, driver, track].filter((v) => v && v !== "—");
    const tail = [type, area].filter((v) => v && v !== "—");
    if (!pieces.length && !tail.length) return "—";
    return `${pieces.join(" | ")}${tail.length ? ` — ${tail.join(" | ")}` : ""}`;
  })();

  const eventDetailsRows = useMemo(() => {
    const details = summary?.event_details;
    if (!details) return [] as Array<{ k: string; v: string }>;

    const fieldOrder = [
      "event_id",
      "day_id",
      "status",
      "creation_time",
      "start_time",
      "end_time",
      "packet_start",
      "packet_end",
      "car_id",
      "driver_id",
      "location_id",
      "event_type",
      "event_index",
      "car_weight",
      "tow_angle",
      "camber_front",
      "camber_rear",
      "toe_front",
      "toe_rear",
      "ride_height_front",
      "ride_height_rear",
      "ride_height",
      "ackerman_adjustment",
      "shock_dampening",
      "power_limit",
      "torque_limit",
      "frw_pressure",
      "flw_pressure",
      "brw_pressure",
      "blw_pressure",
      "fr_wear_depth",
      "fl_wear_depth",
      "rr_wear_depth",
      "rl_wear_depth",
      "fr_durometer",
      "fl_durometer",
      "rr_durometer",
      "rl_durometer",
      "fr_lsc",
      "fr_lsr",
      "fr_hsc",
      "fr_hsr",
      "fl_lsc",
      "fl_lsr",
      "fl_hsc",
      "fl_hsr",
      "rr_lsc",
      "rr_lsr",
      "rr_hsc",
      "rr_hsr",
      "rl_lsc",
      "rl_lsr",
      "rl_hsc",
      "rl_hsr",
      "front_wing_on",
      "rear_wing_on",
      "front_wing_pitch",
      "rear_wing_pitch",
      "regen_on",
      "undertray_on",
      "front_roll_spring_rate",
      "front_heave_spring_rate",
      "rear_roll_spring_rate",
      "rear_heave_spring_rate",
    ] as const;

    const out: Array<{ k: string; v: string }> = [];
    for (const k of fieldOrder) {
      const raw = (details as any)[k];
      if (raw === null || raw === undefined) continue;
      out.push({ k, v: String(raw) });
    }
    return out;
  }, [summary?.event_details]);

  return (
    <>
      <Banner />
      <div className="container mx-auto p-8 pt-24 md:pt-20">
        <div className="grid grid-cols-1 lg:grid-cols-[360px_1fr] gap-8">
          <Card className="h-[calc(100vh-140px)] overflow-hidden">
            <CardHeader>
              <CardTitle>Replay</CardTitle>
            </CardHeader>
            <CardContent className="h-full overflow-auto space-y-4">
              <div className="space-y-2">
                <div className="text-sm font-medium">Drive days</div>
                {driveDays.map((d) => {
                  const isSel = selectedDay?.drive_day.day_id === d.drive_day.day_id;
                  return (
                    <button
                      key={d.drive_day.day_id}
                      className={`w-full text-left border rounded p-2 ${isSel ? "bg-gray-100" : "bg-white"}`}
                      onClick={() => setSelectedDay(isSel ? null : d)}
                    >
                      <div className="font-semibold">Day #{d.drive_day.day_id}</div>
                      <div className="text-xs text-gray-600">
                        {new Date(d.drive_day.date).toLocaleDateString()} | {d.event_count} events
                      </div>
                    </button>
                  );
                })}
              </div>

              {selectedDayInfo ? (
                <div className="space-y-2 border-t pt-3">
                  <div className="text-sm font-medium">Drive day</div>
                  <div className="text-xs text-gray-600 space-y-1">
                    <div>Date: {new Date(selectedDayInfo.date).toLocaleDateString()}</div>
                    <div>Power limit: {selectedDayInfo.power_limit ?? "—"}</div>
                    <div>Air temp: {selectedDayInfo.air_temperature ?? "—"}</div>
                    <div>Rel humidity: {selectedDayInfo.relative_humidity ?? "—"}</div>
                    <div>Track temp: {selectedDayInfo.track_temperature ?? "—"}</div>
                  </div>

                  <div className="pt-2">
                    <div className="text-sm font-medium">Events</div>
                    <div className="space-y-2 mt-2">
                      {eventsLoading ? (
                        <div className="text-sm text-gray-500">Loading events…</div>
                      ) : null}
                      {selectedDayEvents.map((e) => {
                        const isSel = selected?.event_id === e.event_id;
                        return (
                          <button
                            key={e.event_id}
                            className={`w-full text-left border rounded p-2 ${isSel ? "bg-gray-100" : "bg-white"}`}
                            onClick={() => setSelected(isSel ? null : e)}
                          >
                            <div className="font-semibold">Event #{e.event_id}</div>
                            <div className="text-xs text-gray-600">
                              {e.car_name ?? "—"} | {e.driver_name ?? "—"} | {e.track ?? "—"}
                            </div>
                            <div className="text-xs text-gray-500">{e.event_type ?? "—"}</div>
                          </button>
                        );
                      })}
                      {!eventsLoading && !selectedDayEvents.length ? (
                        <div className="text-sm text-gray-500">No events for this drive day.</div>
                      ) : null}
                    </div>
                  </div>
                </div>
              ) : null}
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
                    <div className="text-sm font-medium">Event description</div>
                    <div className="text-sm text-gray-700 mt-1">{eventDescription}</div>
                  </div>

                  <div>
                    <details className="border rounded p-2">
                      <summary className="text-sm font-medium cursor-pointer">Additional Event Info</summary>
                      {eventDetailsRows.length ? (
                        <div className="mt-2 space-y-1">
                          {eventDetailsRows.map((r) => (
                            <div key={r.k} className="grid grid-cols-[180px_1fr] gap-3 text-xs">
                              <div className="font-medium text-gray-700 break-all">{r.k}</div>
                              <div className="text-gray-600 break-all">{r.v}</div>
                            </div>
                          ))}
                        </div>
                      ) : (
                        <div className="mt-2 text-sm text-gray-500">No event fields available.</div>
                      )}
                    </details>
                  </div>

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
                            {new Date(Number(f.start_time)).toLocaleTimeString()} — {f.notes}
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
                      <Button
                        onClick={isEnd ? handleRestart : handleTogglePlay}
                        disabled={timeStart == null}
                      >
                        {isEnd ? "Restart" : playing ? "Pause" : "Play"}
                      </Button>
                    </div>

                    <div className="flex flex-col gap-2">
                      <div className="text-sm text-gray-600">Timeline</div>

                      {timeStart != null && timeEnd != null && timeEnd > timeStart ? (
                        <div className="relative h-6">
                          {markers.map((m, idx) => {
                            const pct = ((m.t - timeStart) / (timeEnd - timeStart)) * 100;
                            if (!Number.isFinite(pct)) return null;

                            const clampedPct = Math.max(0, Math.min(100, pct));
                            // Range inputs typically have an implicit left/right inset equal to ~thumb radius.
                            // Compensate so timeline markers align with the slider's thumb position.
                            const thumbPadPx = 10;

                            // The stream advances in ~100ms ticks, so use a small tolerance window.
                            const isActive =
                              timeCursor != null && Math.abs(timeCursor - m.t) <= 75;

                            return (
                              <button
                                key={`${m.kind}-${m.t}-${idx}`}
                                type="button"
                                className={`absolute top-0 -translate-x-1/2 text-xs select-none cursor-pointer ${
                                  isActive ? "text-orange-500" : "text-gray-700"
                                }`}
                                style={{
                                  left: `calc(${thumbPadPx}px + (${clampedPct} / 100) * (100% - ${thumbPadPx * 2}px))`,
                                }}
                                title={m.label}
                                onClick={() => seekTo(m.t)}
                              >
                                {m.kind === "lap" ? (
                                  <TimelineLapIcon className="h-4 w-4" />
                                ) : (
                                  <TimelineFlagIcon className="h-4 w-4" />
                                )}
                              </button>
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
                          timeCursorRef.current = next ?? null;
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
                <Card className="h-full flex flex-col">
                  <CardHeader><CardTitle>Map</CardTitle></CardHeader>
                  <CardContent className="flex-grow"><DynamicMap resize={false} data={dbMapData} /></CardContent>
                </Card>
              </div>
            ) : null}
          </div>
        </div>
      </div>
    </>
  );
}
