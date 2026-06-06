'use client';

// Interactive editor for the driver's lap-card screen. Drag/resize widgets on a
// scaled 800x480 stage, bind each to a telemetry field, style it, preview live,
// and publish the layout to the car (retained, once). Layout persists locally.

import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { X, Plus, Send, Save, RotateCcw, Trash2, Type, Hash, BarChart3, Gauge as GaugeIcon } from 'lucide-react';
import LapCardRenderer from './LapCardRenderer';
import {
  LAP_CARD_W, LAP_CARD_H, FIELD_CATALOG, fieldDef, defaultLapCardLayout, validateLapCardLayout,
  sampleLapCardData, type LapCardLayout, type Widget, type WidgetType, type FormatId, type Align,
} from '@/lib/dash/dashLayout';

const STORAGE_KEY = 'dash-lap-layout';
const SCALE = 0.82; // 800 -> 656 on the editor stage
const FORMATS: FormatId[] = ['raw', 'int', 'float1', 'float2', 'laptime', 'wh', 'whSigned', 'kwh', 'kw', 'pct', 'temp', 'volt', 'amp', 'mph'];

function loadLayout(): LapCardLayout {
  if (typeof window !== 'undefined') {
    try {
      const raw = localStorage.getItem(STORAGE_KEY);
      if (raw) { const v = validateLapCardLayout(JSON.parse(raw)); if (v) return v; }
    } catch { /* fall through to default */ }
  }
  return defaultLapCardLayout();
}

let widgetSeq = 0;
function newWidget(type: WidgetType): Widget {
  const id = `w${Date.now().toString(36)}${widgetSeq++}`;
  const base = { id, x: 300, y: 200, w: 200, h: 80 };
  if (type === 'text') return { ...base, type, text: 'TEXT', fontSize: 36, color: '#FFFFFF', align: 'center', bold: true };
  if (type === 'value') return { ...base, type, bind: 'lapCard.timeS', format: 'laptime', fontSize: 56, color: '#FFFFFF', align: 'center', bold: true };
  if (type === 'bar') return { ...base, type, h: 40, bind: 'pacing.budgetDeltaWh', min: -100, max: 100, color: '#BF5700', bidirectional: true, label: 'BUDGET Δ' };
  return { ...base, type: 'gauge', w: 180, h: 180, bind: 'can.soc', min: 0, max: 100, label: 'SOC', color: 'gradient', mode: 'standard', format: 'pct' };
}

export function DashLayoutEditor({ onClose, onSend, sendStatus }: {
  onClose: () => void;
  onSend: (layout: LapCardLayout) => void;
  sendStatus?: string;
}) {
  const [layout, setLayout] = useState<LapCardLayout>(loadLayout);
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const data = useMemo(() => sampleLapCardData(), []);
  const stageRef = useRef<HTMLDivElement>(null);
  const drag = useRef<null | { mode: 'move' | 'resize'; id: string; sx: number; sy: number; ox: number; oy: number; ow: number; oh: number }>(null);

  // Persist locally on every change so a refresh keeps your work.
  useEffect(() => {
    try { localStorage.setItem(STORAGE_KEY, JSON.stringify(layout)); } catch { /* quota */ }
  }, [layout]);

  const selected = layout.widgets.find((w) => w.id === selectedId) ?? null;

  const patch = useCallback((id: string, p: Partial<Widget>) => {
    setLayout((l) => ({ ...l, widgets: l.widgets.map((w) => (w.id === id ? { ...w, ...p } as Widget : w)) }));
  }, []);

  // ---- drag + resize ----
  useEffect(() => {
    const onMove = (e: MouseEvent) => {
      const d = drag.current; if (!d) return;
      const dx = (e.clientX - d.sx) / SCALE, dy = (e.clientY - d.sy) / SCALE;
      if (d.mode === 'move') {
        patch(d.id, {
          x: Math.round(Math.min(Math.max(0, d.ox + dx), LAP_CARD_W - d.ow)),
          y: Math.round(Math.min(Math.max(0, d.oy + dy), LAP_CARD_H - d.oh)),
        });
      } else {
        patch(d.id, {
          w: Math.round(Math.max(24, Math.min(d.ow + dx, LAP_CARD_W - d.ox))),
          h: Math.round(Math.max(20, Math.min(d.oh + dy, LAP_CARD_H - d.oy))),
        });
      }
    };
    const onUp = () => { drag.current = null; };
    window.addEventListener('mousemove', onMove);
    window.addEventListener('mouseup', onUp);
    return () => { window.removeEventListener('mousemove', onMove); window.removeEventListener('mouseup', onUp); };
  }, [patch]);

  const startMove = (id: string, e: React.MouseEvent) => {
    e.preventDefault();
    setSelectedId(id);
    const w = layout.widgets.find((x) => x.id === id); if (!w) return;
    drag.current = { mode: 'move', id, sx: e.clientX, sy: e.clientY, ox: w.x, oy: w.y, ow: w.w, oh: w.h };
  };
  const startResize = (e: React.MouseEvent) => {
    e.preventDefault(); e.stopPropagation();
    if (!selected) return;
    drag.current = { mode: 'resize', id: selected.id, sx: e.clientX, sy: e.clientY, ox: selected.x, oy: selected.y, ow: selected.w, oh: selected.h };
  };

  const addWidget = (t: WidgetType) => { const w = newWidget(t); setLayout((l) => ({ ...l, widgets: [...l.widgets, w] })); setSelectedId(w.id); };
  const removeSelected = () => { if (!selected) return; setLayout((l) => ({ ...l, widgets: l.widgets.filter((w) => w.id !== selected.id) })); setSelectedId(null); };
  const resetDefault = () => { if (window.confirm('Reset to the default lap card?')) { setLayout(defaultLapCardLayout()); setSelectedId(null); } };

  return (
    <div className="modalOverlay" onMouseDown={onClose}>
      <div className="dashEditor" onMouseDown={(e) => e.stopPropagation()}>
        <div className="dashEditorHead">
          <div className="dashEditorTitle">
            <strong>Lap-screen designer</strong>
            <input value={layout.name} onChange={(e) => setLayout((l) => ({ ...l, name: e.target.value }))} aria-label="Layout name" />
          </div>
          <div className="dashEditorActions">
            <button className="tool" onClick={resetDefault}><RotateCcw size={14} /> Reset</button>
            <button className="tool" onClick={() => { try { localStorage.setItem(STORAGE_KEY, JSON.stringify(layout)); } catch {} }}><Save size={14} /> Save</button>
            <button className="primary" onClick={() => onSend(layout)}><Send size={14} /> Send to car</button>
            <button className="tool iconOnly" onClick={onClose} aria-label="Close"><X size={16} /></button>
          </div>
        </div>
        {sendStatus ? <div className="dashEditorStatus">{sendStatus}</div> : null}

        <div className="dashEditorBody">
          {/* palette + widget list */}
          <aside className="dashEditorRail">
            <div className="paletteRow">
              <button className="tool" onClick={() => addWidget('text')}><Type size={14} /> Text</button>
              <button className="tool" onClick={() => addWidget('value')}><Hash size={14} /> Value</button>
              <button className="tool" onClick={() => addWidget('bar')}><BarChart3 size={14} /> Bar</button>
              <button className="tool" onClick={() => addWidget('gauge')}><GaugeIcon size={14} /> Gauge</button>
            </div>
            <div className="widgetList">
              {layout.widgets.map((w) => (
                <button key={w.id} className={`widgetItem${w.id === selectedId ? ' sel' : ''}`} onClick={() => setSelectedId(w.id)}>
                  <span className="widgetKind">{w.type}</span>
                  <span className="widgetBind">{w.type === 'text' ? w.text : (w as { bind?: string }).bind}</span>
                </button>
              ))}
              {!layout.widgets.length && <p className="muted" style={{ padding: 8 }}>Add a widget to begin.</p>}
            </div>
          </aside>

          {/* canvas */}
          <div className="dashEditorStage" ref={stageRef} onMouseDown={() => setSelectedId(null)}>
            <div onMouseDown={(e) => e.stopPropagation()} style={{ position: 'relative' }}>
              <LapCardRenderer layout={layout} data={data} scale={SCALE} editable selectedId={selectedId} onWidgetMouseDown={startMove} />
              {selected && (
                <div onMouseDown={startResize} title="Resize"
                  style={{ position: 'absolute', left: (selected.x + selected.w) * SCALE - 7, top: (selected.y + selected.h) * SCALE - 7,
                    width: 14, height: 14, background: '#4ea1ff', borderRadius: 3, cursor: 'nwse-resize', zIndex: 5 }} />
              )}
            </div>
            <p className="muted" style={{ marginTop: 8, fontSize: '0.8rem' }}>800 × 480 · preview uses sample data · drag to move, corner to resize</p>
          </div>

          {/* property panel */}
          <aside className="dashEditorProps">
            {!selected ? <p className="muted" style={{ padding: 8 }}>Select a widget to edit its data &amp; style.</p> : (
              <PropertyPanel w={selected} onChange={(p) => patch(selected.id, p)} onDelete={removeSelected} />
            )}
          </aside>
        </div>
      </div>
    </div>
  );
}

function Row({ label, children }: { label: string; children: React.ReactNode }) {
  return <label className="propRow"><span>{label}</span>{children}</label>;
}

function PropertyPanel({ w, onChange, onDelete }: { w: Widget; onChange: (p: Partial<Widget>) => void; onDelete: () => void }) {
  const hasBind = w.type !== 'text';
  return (
    <div className="propPanel">
      <div className="propHead"><strong>{w.type}</strong><button className="tool dangerTool" onClick={onDelete}><Trash2 size={13} /> Delete</button></div>

      {w.type === 'text' && (
        <Row label="Text"><input value={w.text} onChange={(e) => onChange({ text: e.target.value })} /></Row>
      )}
      {hasBind && (
        <Row label="Data field">
          <select value={(w as { bind: string }).bind} onChange={(e) => {
            const fd = fieldDef(e.target.value);
            const p: Partial<Widget> = { bind: e.target.value } as Partial<Widget>;
            if (fd) {
              if (w.type === 'value' && fd.defaultFormat) (p as { format?: FormatId }).format = fd.defaultFormat;
              if ((w.type === 'bar' || w.type === 'gauge')) {
                if (fd.min != null) (p as { min?: number }).min = fd.min;
                if (fd.max != null) (p as { max?: number }).max = fd.max;
                if (fd.bidirectional != null) (p as { bidirectional?: boolean; mode?: string }).bidirectional = fd.bidirectional;
              }
            }
            onChange(p);
          }}>
            {Array.from(new Set(FIELD_CATALOG.map((f) => f.group))).map((g) => (
              <optgroup key={g} label={g}>
                {FIELD_CATALOG.filter((f) => f.group === g).map((f) => <option key={f.bind} value={f.bind}>{f.label}{f.unit ? ` (${f.unit})` : ''}</option>)}
              </optgroup>
            ))}
          </select>
        </Row>
      )}
      {(w.type === 'value' || w.type === 'gauge') && (
        <Row label="Format">
          <select value={(w as { format?: FormatId }).format ?? 'int'} onChange={(e) => onChange({ format: e.target.value as FormatId } as Partial<Widget>)}>
            {FORMATS.map((f) => <option key={f} value={f}>{f}</option>)}
          </select>
        </Row>
      )}
      {(w.type === 'value' || w.type === 'bar' || w.type === 'gauge') && (
        <Row label="Label"><input value={(w as { label?: string }).label ?? ''} onChange={(e) => onChange({ label: e.target.value } as Partial<Widget>)} /></Row>
      )}
      {(w.type === 'bar' || w.type === 'gauge') && (
        <>
          <Row label="Min"><input type="number" value={(w as { min: number }).min} onChange={(e) => onChange({ min: Number(e.target.value) } as Partial<Widget>)} /></Row>
          <Row label="Max"><input type="number" value={(w as { max: number }).max} onChange={(e) => onChange({ max: Number(e.target.value) } as Partial<Widget>)} /></Row>
        </>
      )}
      {(w.type === 'text' || w.type === 'value') && (
        <Row label="Font size"><input type="number" value={w.fontSize} onChange={(e) => onChange({ fontSize: Number(e.target.value) } as Partial<Widget>)} /></Row>
      )}
      {w.type !== 'bar' && (
        <Row label="Color"><input type="color" value={normalizeColor((w as { color?: string }).color)} onChange={(e) => onChange({ color: e.target.value } as Partial<Widget>)} /></Row>
      )}
      {w.type === 'bar' && (
        <Row label="Fill color"><input type="color" value={normalizeColor(w.color)} onChange={(e) => onChange({ color: e.target.value })} /></Row>
      )}
      {(w.type === 'text' || w.type === 'value') && (
        <Row label="Align">
          <select value={(w as { align?: Align }).align ?? 'center'} onChange={(e) => onChange({ align: e.target.value as Align } as Partial<Widget>)}>
            <option value="left">left</option><option value="center">center</option><option value="right">right</option>
          </select>
        </Row>
      )}
      <div className="propGrid">
        <Row label="X"><input type="number" value={w.x} onChange={(e) => onChange({ x: Number(e.target.value) })} /></Row>
        <Row label="Y"><input type="number" value={w.y} onChange={(e) => onChange({ y: Number(e.target.value) })} /></Row>
        <Row label="W"><input type="number" value={w.w} onChange={(e) => onChange({ w: Number(e.target.value) })} /></Row>
        <Row label="H"><input type="number" value={w.h} onChange={(e) => onChange({ h: Number(e.target.value) })} /></Row>
      </div>
    </div>
  );
}

function normalizeColor(c?: string): string {
  if (!c || c === 'gradient') return '#ffffff';
  return c.startsWith('#') ? c : '#ffffff';
}

export default DashLayoutEditor;
