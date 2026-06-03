"use client";

import { useCallback, useEffect, useRef, useState } from "react";
import { Activity, AlertTriangle, BatteryCharging, Clock, Gauge, Power, Radio, SlidersHorizontal, Zap } from "lucide-react";
import "./car-status.css";

// Mirrors the processor's car_status message shape.
type CarStatusEvent = {
  kind?: string; // "transition" | "heartbeat" | "status"
  ok?: boolean;
  message?: string;
  car?: string;
  state?: CarState;
  reasons?: string[];
  active_faults?: string[];
  time_in_state_ms?: number;
  hv_soc?: number | null;
  hv_pack_v?: number | null;
  lv_v?: number | null;
  lv_c?: number | null;
  lv_t?: number | null;
  thresholds?: Record<string, number>;
  t_ms?: number;
};

type CarState = "OFF" | "ON_IDLE" | "READY" | "MOVING";

type HistorySegment = {
  id: number;
  state: CarState;
  startMs: number;
  endMs: number | null;
  durationMs: number | null;
  hvSocAvg: number | null;
  lvVAvg: number | null;
  activeFaults: string[];
};
type HistoryResponse = {
  segments: HistorySegment[];
  totals: { movingMs: number; movingCount: number; byState: Record<string, number> };
  fromMs: number;
  toMs: number;
};
const RANGES: { label: string; ms: number }[] = [
  { label: "1h", ms: 60 * 60 * 1000 },
  { label: "24h", ms: 24 * 60 * 60 * 1000 },
  { label: "7d", ms: 7 * 24 * 60 * 60 * 1000 },
];

const STATE_META: Record<CarState, { label: string; tone: string; icon: React.ReactNode }> = {
  OFF: { label: "Off", tone: "off", icon: <Power size={20} /> },
  ON_IDLE: { label: "On — Idle", tone: "idle", icon: <Activity size={20} /> },
  READY: { label: "Ready to Drive", tone: "ready", icon: <Radio size={20} /> },
  MOVING: { label: "Moving", tone: "moving", icon: <Gauge size={20} /> },
};

// Slider definitions mirror the processor's Thresholds dataclass.
const SLIDERS: { key: string; label: string; min: number; max: number; step: number; unit: string }[] = [
  { key: "hv_live_v", label: "HV live above", min: 0, max: 200, step: 1, unit: "V" },
  { key: "move_rpm", label: "Moving · motor rpm", min: 0, max: 1000, step: 5, unit: "rpm" },
  { key: "move_wheel", label: "Moving · wheel speed", min: 0, max: 20, step: 0.1, unit: "rad/s" },
  { key: "move_mps", label: "Moving · GPS speed", min: 0, max: 20, step: 0.1, unit: "m/s" },
  { key: "min_state_ms", label: "Debounce", min: 0, max: 3000, step: 50, unit: "ms" },
];

const fmt = (v: number | null | undefined, digits = 1) =>
  v === null || v === undefined || !Number.isFinite(v) ? "—" : v.toFixed(digits);

function formatDuration(ms: number | null | undefined): string {
  if (!ms || ms < 0) return "0s";
  const s = Math.floor(ms / 1000);
  if (s < 60) return `${s}s`;
  const m = Math.floor(s / 60);
  if (m < 60) return `${m}m ${s % 60}s`;
  const h = Math.floor(m / 60);
  return `${h}h ${m % 60}m`;
}

function formatClock(ms: number): string {
  return new Date(ms).toLocaleString([], {
    month: "short", day: "numeric", hour: "2-digit", minute: "2-digit", second: "2-digit",
  });
}

const STATE_COLOR: Record<string, string> = {
  OFF: "var(--cs-off)",
  ON_IDLE: "var(--cs-idle)",
  READY: "var(--cs-ready)",
  MOVING: "var(--cs-moving)",
};

export default function CarStatusApp() {
  const [car, setCar] = useState<"orion" | "angelique">("orion");
  const [connected, setConnected] = useState(false);
  const [statusMsg, setStatusMsg] = useState("Connecting…");
  const [latest, setLatest] = useState<CarStatusEvent | null>(null);
  const [thresholds, setThresholds] = useState<Record<string, number>>({});
  const [sliderDraft, setSliderDraft] = useState<Record<string, number>>({});
  const [dirty, setDirty] = useState(false);
  const [savedAt, setSavedAt] = useState<number | null>(null);
  const sourceRef = useRef<EventSource | null>(null);

  // History
  const [rangeMs, setRangeMs] = useState(RANGES[1].ms); // default 24h
  const [history, setHistory] = useState<HistoryResponse | null>(null);
  const [historyMsg, setHistoryMsg] = useState("");

  const loadHistory = useCallback(async () => {
    setHistoryMsg("Loading…");
    try {
      const toMs = Date.now();
      const fromMs = toMs - rangeMs;
      const res = await fetch(`/api/car-status/history?car=${encodeURIComponent(car)}&from=${fromMs}&to=${toMs}`);
      if (!res.ok) {
        setHistory(null);
        setHistoryMsg(`History unavailable (${res.status}).`);
        return;
      }
      setHistory((await res.json()) as HistoryResponse);
      setHistoryMsg("");
    } catch (e) {
      setHistory(null);
      setHistoryMsg(e instanceof Error ? e.message : String(e));
    }
  }, [car, rangeMs]);

  useEffect(() => {
    void loadHistory();
  }, [loadHistory]);

  // Live SSE connection (reconnects when the car filter changes).
  useEffect(() => {
    sourceRef.current?.close();
    setConnected(false);
    setStatusMsg("Connecting…");
    const es = new EventSource(`/api/car-status/stream?car=${encodeURIComponent(car)}`);
    sourceRef.current = es;
    es.onmessage = (ev) => {
      let msg: CarStatusEvent;
      try {
        msg = JSON.parse(ev.data) as CarStatusEvent;
      } catch {
        return;
      }
      if (msg.kind === "status") {
        setConnected(!!msg.ok);
        setStatusMsg(msg.message || (msg.ok ? "Listening" : "Unavailable"));
        return;
      }
      setConnected(true);
      setLatest(msg);
      if (msg.thresholds) {
        setThresholds(msg.thresholds);
        // Only adopt processor thresholds into the sliders when the user hasn't
        // started editing, so live updates don't yank the slider under them.
        setSliderDraft((prev) => (Object.keys(prev).length === 0 ? msg.thresholds! : prev));
      }
    };
    es.onerror = () => {
      setConnected(false);
      setStatusMsg("Live stream interrupted.");
    };
    return () => es.close();
  }, [car]);

  const setSlider = useCallback((key: string, value: number) => {
    setSliderDraft((prev) => ({ ...prev, [key]: value }));
    setDirty(true);
  }, []);

  const applyThresholds = useCallback(async () => {
    try {
      const res = await fetch("/api/car-status/config", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(sliderDraft),
      });
      if (res.ok) {
        setSavedAt(Date.now());
        setDirty(false);
      } else {
        setStatusMsg(`Threshold update failed (${res.status}).`);
      }
    } catch (e) {
      setStatusMsg(e instanceof Error ? e.message : String(e));
    }
  }, [sliderDraft]);

  const resetSliders = useCallback(() => {
    setSliderDraft(thresholds);
    setDirty(false);
  }, [thresholds]);

  const state = (latest?.state as CarState) || "OFF";
  const meta = STATE_META[state] ?? STATE_META.OFF;
  const hvSoc = latest?.hv_soc ?? null;
  const lvV = latest?.lv_v ?? null;

  return (
    <main className="csShell">
      <header className="csTopbar">
        <div>
          <h1>Car Status</h1>
          <p>Central live state · HV state-of-charge · LV battery voltage</p>
        </div>
        <div className="csTopActions">
          <select value={car} onChange={(e) => setCar(e.target.value as "orion" | "angelique")}>
            <option value="orion">Orion</option>
            <option value="angelique">Angelique</option>
          </select>
          <span className={connected ? "csConn csOn" : "csConn"}>
            <Radio size={13} /> {connected ? "Live" : "Offline"}
          </span>
        </div>
      </header>

      <section className="csHero">
        <div className={`csBadge cs-${meta.tone}`}>
          <span className="csBadgeIcon">{meta.icon}</span>
          <div>
            <span className="csBadgeLabel">{meta.label}</span>
            <span className="csBadgeSub">for {formatDuration(latest?.time_in_state_ms)}</span>
          </div>
        </div>

        <div className="csEnergy">
          <div className="csGauge">
            <span className="csGaugeLabel"><BatteryCharging size={15} /> HV SoC</span>
            <span className="csGaugeValue">{fmt(hvSoc, 1)}<small>%</small></span>
            <div className="csBar"><div className="csBarFill cs-hv" style={{ width: `${Math.max(0, Math.min(100, hvSoc ?? 0))}%` }} /></div>
            <span className="csGaugeSub">{fmt(latest?.hv_pack_v, 1)} V pack</span>
          </div>
          <div className="csGauge">
            <span className="csGaugeLabel"><Zap size={15} /> LV Battery</span>
            <span className="csGaugeValue">{fmt(lvV, 2)}<small>V</small></span>
            <span className="csGaugeSub">
              {fmt(latest?.lv_c, 1)} A · {fmt(latest?.lv_t, 1)} °C
            </span>
            <span className="csGaugeNote">no LV SoC signal — voltage shown</span>
          </div>
        </div>
      </section>

      <section className="csReasons">
        <span className="csReasonsLabel">Why this state</span>
        <div className="csChips">
          {(latest?.reasons?.length ? latest.reasons : ["—"]).map((r) => (
            <span key={r} className="csChip">{r}</span>
          ))}
        </div>
        <small className="csStatusLine">{statusMsg}</small>
      </section>

      {latest?.active_faults?.length ? (
        <section className="csFaults" role="alert">
          <AlertTriangle size={16} />
          <span className="csFaultsLabel">Active faults</span>
          <div className="csChips">
            {latest.active_faults.map((fault) => (
              <span key={fault} className="csChip csChipFault">{fault}</span>
            ))}
          </div>
          <small>Advisory — does not change the state.</small>
        </section>
      ) : null}

      <section className="csTune">
        <div className="csTuneHead">
          <SlidersHorizontal size={16} />
          <h2>Classification thresholds</h2>
          <small>Live-tunable — changes apply to the processor immediately.</small>
        </div>
        <div className="csSliders">
          {SLIDERS.map((s) => {
            const val = sliderDraft[s.key] ?? thresholds[s.key] ?? s.min;
            return (
              <label key={s.key} className="csSlider">
                <span className="csSliderTop">
                  <span>{s.label}</span>
                  <strong>{val}{s.unit && <small> {s.unit}</small>}</strong>
                </span>
                <input
                  type="range"
                  min={s.min}
                  max={s.max}
                  step={s.step}
                  value={val}
                  onChange={(e) => setSlider(s.key, Number(e.target.value))}
                />
              </label>
            );
          })}
        </div>
        <div className="csTuneActions">
          <button className="csPrimary" disabled={!dirty} onClick={applyThresholds}>Apply live</button>
          <button className="csTool" disabled={!dirty} onClick={resetSliders}>Reset</button>
          {savedAt ? <small className="csSaved">Applied</small> : null}
        </div>
      </section>

      <section className="csHistory">
        <div className="csHistoryHead">
          <Clock size={16} />
          <h2>History</h2>
          <div className="csRanges">
            {RANGES.map((r) => (
              <button
                key={r.label}
                className={rangeMs === r.ms ? "csRange csRangeOn" : "csRange"}
                onClick={() => setRangeMs(r.ms)}
              >
                {r.label}
              </button>
            ))}
            <button className="csTool" onClick={() => void loadHistory()}>Refresh</button>
          </div>
        </div>

        {history?.segments?.length ? (
          <>
            <div className="csTimeline" title="State over time">
              {history.segments.map((seg) => {
                const span = Math.max(1, history.toMs - history.fromMs);
                const start = Math.max(seg.startMs, history.fromMs);
                const end = Math.min(seg.endMs ?? history.toMs, history.toMs);
                const left = ((start - history.fromMs) / span) * 100;
                const width = Math.max(0.2, ((end - start) / span) * 100);
                return (
                  <span
                    key={seg.id}
                    className="csTimelineSeg"
                    style={{ left: `${left}%`, width: `${width}%`, background: STATE_COLOR[seg.state] ?? "var(--cs-off)" }}
                    title={`${seg.state} · ${formatClock(seg.startMs)} · ${formatDuration(seg.durationMs)}`}
                  />
                );
              })}
            </div>

            <div className="csHistTotals">
              <span className="csTotalMoving">
                Moving: <strong>{formatDuration(history.totals.movingMs)}</strong> across{" "}
                {history.totals.movingCount} window{history.totals.movingCount === 1 ? "" : "s"}
              </span>
              {Object.entries(history.totals.byState).map(([st, ms]) => (
                <span key={st} className="csTotalChip" style={{ borderColor: STATE_COLOR[st] ?? "var(--cs-border)" }}>
                  {st} {formatDuration(ms)}
                </span>
              ))}
            </div>

            <div className="csTableWrap">
              <table className="csTable">
                <thead>
                  <tr><th>State</th><th>Start</th><th>End</th><th>Duration</th><th>HV SoC</th><th>LV V</th><th>Faults</th></tr>
                </thead>
                <tbody>
                  {history.segments.slice().reverse().map((seg) => (
                    <tr key={seg.id} className={seg.state === "MOVING" ? "csRowMoving" : ""}>
                      <td><span className="csStateDot" style={{ background: STATE_COLOR[seg.state] }} /> {seg.state}</td>
                      <td>{formatClock(seg.startMs)}</td>
                      <td>{seg.endMs === null ? "—" : formatClock(seg.endMs)}</td>
                      <td>{formatDuration(seg.durationMs)}</td>
                      <td>{fmt(seg.hvSocAvg, 1)}</td>
                      <td>{fmt(seg.lvVAvg, 2)}</td>
                      <td>{seg.activeFaults.length ? seg.activeFaults.join(", ") : "—"}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </>
        ) : (
          <p className="csHistoryEmpty">{historyMsg || "No segments in this range."}</p>
        )}
      </section>
    </main>
  );
}
