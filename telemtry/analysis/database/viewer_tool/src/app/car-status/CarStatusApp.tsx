"use client";

import { useCallback, useEffect, useRef, useState } from "react";
import { Activity, AlertTriangle, BatteryCharging, Gauge, Power, Radio, SlidersHorizontal, Zap } from "lucide-react";
import "./car-status.css";

// Mirrors the processor's car_status message shape.
type CarStatusEvent = {
  kind?: string; // "transition" | "heartbeat" | "status"
  ok?: boolean;
  message?: string;
  car?: string;
  state?: CarState;
  reasons?: string[];
  time_in_state_ms?: number;
  hv_soc?: number | null;
  hv_pack_v?: number | null;
  lv_v?: number | null;
  lv_c?: number | null;
  lv_t?: number | null;
  thresholds?: Record<string, number>;
  t_ms?: number;
};

type CarState = "OFF" | "ON_IDLE" | "READY" | "MOVING" | "FAULT";

const STATE_META: Record<CarState, { label: string; tone: string; icon: React.ReactNode }> = {
  OFF: { label: "Off", tone: "off", icon: <Power size={20} /> },
  ON_IDLE: { label: "On — Idle", tone: "idle", icon: <Activity size={20} /> },
  READY: { label: "Ready to Drive", tone: "ready", icon: <Radio size={20} /> },
  MOVING: { label: "Moving", tone: "moving", icon: <Gauge size={20} /> },
  FAULT: { label: "Fault", tone: "fault", icon: <AlertTriangle size={20} /> },
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

      <p className="csFootnote">
        Phase 1: live classification only. History timeline, &ldquo;moving windows&rdquo; and
        CSV tagging land in later phases (see the design doc).
      </p>
    </main>
  );
}
