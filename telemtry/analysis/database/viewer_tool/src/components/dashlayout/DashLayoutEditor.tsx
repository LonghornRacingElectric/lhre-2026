'use client';

// Interactive editor for the driver's lap-card screen. Drag/resize widgets on a
// scaled 800x480 stage, bind each to a telemetry field, style it, preview live,
// and publish the layout to the car (retained, once). Layout persists locally.

import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { X, Send, Plus, Copy, RotateCcw, Trash2, Type, Hash, BarChart3, Gauge as GaugeIcon, TrendingUp, RefreshCw, Cloud } from 'lucide-react';
import LapCardRenderer from './LapCardRenderer';
import { fetchServerLayouts, saveServerLayouts } from '@/lib/dash/layoutStore';
import {
  LAP_CARD_W, LAP_CARD_H, fieldDef, validateLapCardLayout,
  sampleLapCardData, LAYOUT_TEMPLATES, DASH_SCREENS, screenDef, fieldsForScreen,
  type LapCardLayout, type Widget, type WidgetType, type FieldDef,
  type FormatId, type Align, type GoodSign, type ColorRule,
} from '@/lib/dash/dashLayout';

const STORAGE_KEY = 'dash-lap-layout';   // legacy single-layout key (migrated on load)
const SCALE = 0.82; // 800 -> 656 on the editor stage
const GRID = 10;        // snap-to-grid step (layout px)
const SNAP_THRESH = 7;  // alignment-guide snap distance (layout px)

type Guide = { o: 'v' | 'h'; pos: number };

// Snap a moving widget against the grid + canvas/sibling edges & centers.
// Returns the adjusted x/y and the guide lines to draw.
function snapMove(nx: number, ny: number, ow: number, oh: number, others: Widget[], grid: boolean): { x: number; y: number; guides: Guide[] } {
  if (grid) { nx = Math.round(nx / GRID) * GRID; ny = Math.round(ny / GRID) * GRID; }
  const guides: Guide[] = [];
  const vTargets = [0, LAP_CARD_W / 2, LAP_CARD_W, ...others.flatMap((w) => [w.x, w.x + w.w / 2, w.x + w.w])];
  const hTargets = [0, LAP_CARD_H / 2, LAP_CARD_H, ...others.flatMap((w) => [w.y, w.y + w.h / 2, w.y + w.h])];
  // x: try left / center / right anchors, snap to nearest target
  let bestX: { shift: number; pos: number } | null = null;
  for (const a of [nx, nx + ow / 2, nx + ow]) for (const t of vTargets) {
    const d = t - a; if (Math.abs(d) <= SNAP_THRESH && (!bestX || Math.abs(d) < Math.abs(bestX.shift))) bestX = { shift: d, pos: t };
  }
  if (bestX) { nx += bestX.shift; guides.push({ o: 'v', pos: bestX.pos }); }
  let bestY: { shift: number; pos: number } | null = null;
  for (const a of [ny, ny + oh / 2, ny + oh]) for (const t of hTargets) {
    const d = t - a; if (Math.abs(d) <= SNAP_THRESH && (!bestY || Math.abs(d) < Math.abs(bestY.shift))) bestY = { shift: d, pos: t };
  }
  if (bestY) { ny += bestY.shift; guides.push({ o: 'h', pos: bestY.pos }); }
  return {
    x: Math.round(Math.min(Math.max(0, nx), LAP_CARD_W - ow)),
    y: Math.round(Math.min(Math.max(0, ny), LAP_CARD_H - oh)),
    guides,
  };
}
const FORMATS: FormatId[] = ['raw', 'int', 'float1', 'float2', 'laptime', 'wh', 'whSigned', 'kwh', 'kw', 'pct', 'temp', 'volt', 'amp', 'mph'];

function genLayoutId(): string {
  return `lay-${Date.now().toString(36)}-${Math.floor(Math.random() * 1e6).toString(36)}`;
}
function withId(l: LapCardLayout): LapCardLayout { return l.id ? l : { ...l, id: genLayoutId() }; }

interface Library { items: LapCardLayout[]; activeId: string; }

// Load a screen's saved layout library from its localStorage namespace. The lap
// card also migrates the legacy single-layout key; other screens start from
// their built-in default.
function loadLibrary(screenId: string): Library {
  const sd = screenDef(screenId);
  if (typeof window !== 'undefined') {
    try {
      const raw = localStorage.getItem(sd.libraryKey);
      if (raw) {
        const parsed = JSON.parse(raw) as { items?: unknown[]; activeId?: string };
        const items = (parsed.items ?? [])
          .map((i) => validateLapCardLayout(i)).filter(Boolean)
          .map((l) => withId(l as LapCardLayout)) as LapCardLayout[];
        if (items.length) {
          const activeId = items.some((i) => i.id === parsed.activeId) ? parsed.activeId! : items[0].id!;
          return { items, activeId };
        }
      }
      if (screenId === 'lapCard') {
        const old = localStorage.getItem(STORAGE_KEY); // migrate legacy single layout
        if (old) { const v = validateLapCardLayout(JSON.parse(old)); if (v) { const w = withId(v); return { items: [w], activeId: w.id! }; } }
      }
    } catch { /* fall through to default */ }
  }
  const d = withId(sd.build());
  return { items: [d], activeId: d.id! };
}

let widgetSeq = 0;
function newWidget(type: WidgetType): Widget {
  const id = `w${Date.now().toString(36)}${widgetSeq++}`;
  const base = { id, x: 300, y: 200, w: 200, h: 80 };
  if (type === 'text') return { ...base, type, text: 'TEXT', fontSize: 36, color: '#FFFFFF', align: 'center', bold: true };
  if (type === 'value') return { ...base, type, bind: 'lapCard.timeS', format: 'laptime', fontSize: 56, color: '#FFFFFF', align: 'center', bold: true };
  if (type === 'bar') return { ...base, type, h: 40, bind: 'pacing.budgetDeltaWh', min: -100, max: 100, color: '#BF5700', bidirectional: true, signColored: true, goodSign: 'negative', label: 'BUDGET Δ' };
  if (type === 'delta') return { ...base, type, bind: 'mqtt.lapDelta', format: 'float2', fontSize: 64, align: 'center', bold: true, label: 'LAP Δ', goodSign: 'negative', showArrow: true, showSign: true };
  return { ...base, type: 'gauge', w: 180, h: 180, bind: 'can.soc', min: 0, max: 100, label: 'SOC', color: 'gradient', mode: 'standard', format: 'pct' };
}

export function DashLayoutEditor({ onClose, onSend, sendStatus, initialScreenId }: {
  onClose: () => void;
  onSend: (screenId: string, layout: LapCardLayout) => void;
  sendStatus?: string;
  initialScreenId?: string;
}) {
  // Which dash screen is being authored (lap card / park / …). Each has its own
  // layout library + publish topic; switching reloads that screen's library.
  const [screenId, setScreenId] = useState<string>(initialScreenId ?? 'lapCard');
  const [lib, setLib] = useState<Library>(() => loadLibrary(initialScreenId ?? 'lapCard'));
  // The screen `lib` currently holds — so the persist effect writes to the right
  // namespace even on the render where screenId has changed but lib hasn't yet.
  const libScreenRef = useRef<string>(initialScreenId ?? 'lapCard');
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const [snap, setSnap] = useState(true);
  const [guides, setGuides] = useState<Guide[]>([]);
  const [previewTheme, setPreviewTheme] = useState<'dark' | 'light'>('dark');
  const data = useMemo(() => sampleLapCardData(), []);
  const stageRef = useRef<HTMLDivElement>(null);
  const fields = useMemo(() => fieldsForScreen(screenId), [screenId]);
  // Templates offered for the active screen. The lap card has its curated set;
  // other screens just offer their built-in default as a starting point.
  const screenTemplates = useMemo(
    () => (screenId === 'lapCard'
      ? LAYOUT_TEMPLATES
      : [{ id: 'default', name: `Default ${screenDef(screenId).name}`, build: screenDef(screenId).build }]),
    [screenId],
  );

  // The active layout being edited. setLayout updates just that library entry,
  // so all the existing widget handlers keep working unchanged.
  const layout = lib.items.find((l) => l.id === lib.activeId) ?? lib.items[0] ?? withId(screenDef(screenId).build());
  const setLayout = useCallback((upd: LapCardLayout | ((p: LapCardLayout) => LapCardLayout)) => {
    setLib((prev) => ({
      ...prev,
      items: prev.items.map((l) => (l.id === prev.activeId ? (typeof upd === 'function' ? upd(l) : upd) : l)),
    }));
  }, []);

  const layoutRef = useRef(layout);
  layoutRef.current = layout;
  const snapRef = useRef(snap);
  snapRef.current = snap;
  const drag = useRef<null | { mode: 'move' | 'resize'; id: string; sx: number; sy: number; ox: number; oy: number; ow: number; oh: number }>(null);

  const [syncMsg, setSyncMsg] = useState('Syncing…');
  // The server library version we last loaded — sent with each save so the
  // server can reject (409) if another editor saved in between, rather than us
  // silently clobbering their work. null = unknown (first save / offline).
  const serverSavedAtRef = useRef<number | null>(null);

  // Push the library to the shared server with optimistic concurrency. On a
  // conflict the server returns its current copy; we MERGE (server as the base,
  // our actively-edited layout wins, plus any layouts only we have) and re-save
  // on the fresh version — so concurrent edits to *different* layouts both
  // survive, and only simultaneous edits to the *same* layout are last-wins.
  const pushToServer = useCallback(async (screen: string, items: LapCardLayout[], activeId: string) => {
    const r = await saveServerLayouts(screen, items, serverSavedAtRef.current);
    if (r.ok) { serverSavedAtRef.current = r.savedAt ?? serverSavedAtRef.current; setSyncMsg('Synced'); return; }
    if (r.conflict && r.items) {
      const byId = new Map<string, LapCardLayout>(r.items.map(withId).map((l) => [l.id!, l]));
      for (const l of items) if (l.id === activeId || !byId.has(l.id!)) byId.set(l.id!, withId(l));
      const merged = Array.from(byId.values());
      serverSavedAtRef.current = r.savedAt ?? null; // adopt the server's new base
      setSyncMsg('Merged changes from another editor');
      // setLib re-triggers the debounced effect, which re-saves on the fresh base.
      setLib((prev) => ({ items: merged, activeId: merged.some((i) => i.id === prev.activeId) ? prev.activeId : (merged[0]?.id ?? prev.activeId) }));
    }
  }, []);

  // Persist locally on every change; debounce a push to the shared server copy
  // so every client sees the same saved layouts.
  const serverSaveTimer = useRef<number | null>(null);
  useEffect(() => {
    const key = screenDef(libScreenRef.current).libraryKey;
    const screen = libScreenRef.current;
    const snapshot = lib;
    try { localStorage.setItem(key, JSON.stringify(snapshot)); } catch { /* quota */ }
    if (serverSaveTimer.current) window.clearTimeout(serverSaveTimer.current);
    serverSaveTimer.current = window.setTimeout(() => { void pushToServer(screen, snapshot.items, snapshot.activeId); }, 800);
    return () => { if (serverSaveTimer.current) window.clearTimeout(serverSaveTimer.current); };
  }, [lib, pushToServer]);

  // Pull a screen's shared library. Server is the source of truth if it has
  // anything; if it's empty but we have local layouts, seed it from local.
  const pullFromServer = useCallback(async (screen: string, localItems: LapCardLayout[], announce = true) => {
    if (announce) setSyncMsg('Syncing…');
    const server = await fetchServerLayouts(screen);
    if (server === null) { setSyncMsg('Offline — local only'); return; }
    serverSavedAtRef.current = server.savedAt; // remember the base we loaded
    if (server.items.length) {
      const items = server.items.map(withId);
      libScreenRef.current = screen;
      setLib((prev) => ({ items, activeId: items.some((i) => i.id === prev.activeId) ? prev.activeId : items[0].id! }));
      setSyncMsg('Synced');
    } else {
      // server empty — seed it from whatever we have locally
      setSyncMsg('Synced');
      const r = await saveServerLayouts(screen, localItems, server.savedAt);
      if (r.ok) serverSavedAtRef.current = r.savedAt ?? serverSavedAtRef.current;
    }
  }, []);
  useEffect(() => { void pullFromServer(screenId, lib.items); /* on open only */ // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Switch the screen being authored: load that screen's library + pull its
  // shared copy. Persisting the outgoing screen already happened via the effect.
  const switchScreen = useCallback((id: string) => {
    if (id === screenId) return;
    const next = loadLibrary(id);
    libScreenRef.current = id;
    setScreenId(id);
    setLib(next);
    setSelectedId(null);
    void pullFromServer(id, next.items);
  }, [screenId, pullFromServer]);

  const uniqueName = (base: string): string => {
    const names = new Set(lib.items.map((l) => l.name));
    if (!names.has(base)) return base;
    let n = 2; while (names.has(`${base} ${n}`)) n++; return `${base} ${n}`;
  };
  const addLayout = (l: LapCardLayout) => {
    const w = withId({ ...l, id: undefined, name: uniqueName(l.name || screenDef(screenId).name) });
    setLib((p) => ({ items: [...p.items, w], activeId: w.id! }));
    setSelectedId(null);
  };
  const deleteActive = () => {
    if (!window.confirm(`Delete layout “${layout.name}”?`)) return;
    setLib((p) => {
      const items = p.items.filter((l) => l.id !== p.activeId);
      if (!items.length) { const d = withId(screenDef(screenId).build()); return { items: [d], activeId: d.id! }; }
      return { items, activeId: items[0].id! };
    });
    setSelectedId(null);
  };

  const selected = layout.widgets.find((w) => w.id === selectedId) ?? null;

  const patch = useCallback((id: string, p: Partial<Widget>) => {
    setLayout((l) => ({ ...l, widgets: l.widgets.map((w) => (w.id === id ? { ...w, ...p } as Widget : w)) }));
  }, [setLayout]);

  // ---- drag + resize ----
  useEffect(() => {
    const onMove = (e: MouseEvent) => {
      const d = drag.current; if (!d) return;
      const dx = (e.clientX - d.sx) / SCALE, dy = (e.clientY - d.sy) / SCALE;
      if (d.mode === 'move') {
        const others = layoutRef.current.widgets.filter((w) => w.id !== d.id);
        const r = snapMove(d.ox + dx, d.oy + dy, d.ow, d.oh, others, snapRef.current);
        setGuides(r.guides);
        patch(d.id, { x: r.x, y: r.y });
      } else {
        let nw = d.ow + dx, nh = d.oh + dy;
        if (snapRef.current) { nw = Math.round(nw / GRID) * GRID; nh = Math.round(nh / GRID) * GRID; }
        patch(d.id, {
          w: Math.round(Math.max(24, Math.min(nw, LAP_CARD_W - d.ox))),
          h: Math.round(Math.max(20, Math.min(nh, LAP_CARD_H - d.oy))),
        });
      }
    };
    const onUp = () => { drag.current = null; setGuides([]); };
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
  const resetDefault = () => { if (window.confirm(`Reset this layout to the default ${screenDef(screenId).name}?`)) { setLayout((l) => ({ ...screenDef(screenId).build(), id: l.id, name: l.name })); setSelectedId(null); } };

  return (
    <div className="modalOverlay" onMouseDown={onClose}>
      <div className="dashEditor" onMouseDown={(e) => e.stopPropagation()}>
        <div className="dashEditorHead">
          <div className="dashEditorTitle">
            <strong>Dash designer</strong>
            <select className="tmplSelect" value={screenId} aria-label="Screen"
              title="Which dash screen to design" onChange={(e) => switchScreen(e.target.value)}>
              {DASH_SCREENS.map((s) => <option key={s.id} value={s.id}>{s.name}</option>)}
            </select>
            <select className="tmplSelect" value={lib.activeId} aria-label="Saved layout"
              onChange={(e) => { setLib((p) => ({ ...p, activeId: e.target.value })); setSelectedId(null); }}>
              {lib.items.map((l) => <option key={l.id} value={l.id}>{l.name}</option>)}
            </select>
            <input value={layout.name} onChange={(e) => setLayout((l) => ({ ...l, name: e.target.value }))} aria-label="Layout name" title="Rename this layout" />
            <button className="tool" onClick={() => addLayout(screenDef(screenId).build())} title="New layout"><Plus size={14} /> New</button>
            <button className="tool" onClick={() => addLayout({ ...layout, name: `${layout.name} copy` })} title="Duplicate"><Copy size={14} /></button>
            <button className="tool dangerTool" disabled={lib.items.length <= 1} onClick={deleteActive} title="Delete layout"><Trash2 size={14} /></button>
          </div>
          <div className="dashEditorActions">
            <span className="syncChip" title="Saved layouts are shared with all clients"><Cloud size={13} /> {syncMsg}</span>
            <button className="tool iconOnly" title="Refresh from server" onClick={() => pullFromServer(screenId, lib.items)}><RefreshCw size={14} /></button>
            <select className="tmplSelect" value="" aria-label="New from template"
              onChange={(e) => {
                const t = screenTemplates.find((x) => x.id === e.target.value);
                if (t) addLayout(t.build());
                e.target.value = '';
              }}>
              <option value="">New from template…</option>
              {screenTemplates.map((t) => <option key={t.id} value={t.id}>{t.name}</option>)}
            </select>
            <label className="checkInline" style={{ whiteSpace: 'nowrap' }}>
              <input type="checkbox" checked={snap} onChange={(e) => setSnap(e.target.checked)} /> Snap
            </label>
            <button className="tool" onClick={resetDefault}><RotateCcw size={14} /> Reset</button>
            <button className="primary" onClick={() => onSend(screenId, layout)}><Send size={14} /> Send to car</button>
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
              <button className="tool" onClick={() => addWidget('delta')}><TrendingUp size={14} /> Delta</button>
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
          <div className="dashEditorStage" ref={stageRef}
            style={{ background: previewTheme === 'light' ? '#d9d6d2' : '#0b0c0e' }}
            onMouseDown={() => setSelectedId(null)}>
            <div onMouseDown={(e) => e.stopPropagation()} style={{ position: 'relative' }}>
              <LapCardRenderer layout={layout} data={data} scale={SCALE} theme={previewTheme} editable selectedId={selectedId} onWidgetMouseDown={startMove} />
              {guides.map((g, i) => (
                <div key={i} style={{ position: 'absolute', background: '#4ea1ff', pointerEvents: 'none', zIndex: 6,
                  ...(g.o === 'v'
                    ? { left: g.pos * SCALE, top: 0, width: 1, height: LAP_CARD_H * SCALE }
                    : { top: g.pos * SCALE, left: 0, height: 1, width: LAP_CARD_W * SCALE }) }} />
              ))}
              {selected && (
                <div onMouseDown={startResize} title="Resize"
                  style={{ position: 'absolute', left: (selected.x + selected.w) * SCALE - 7, top: (selected.y + selected.h) * SCALE - 7,
                    width: 14, height: 14, background: '#4ea1ff', borderRadius: 3, cursor: 'nwse-resize', zIndex: 5 }} />
              )}
            </div>
            <div style={{ marginTop: 8, display: 'flex', alignItems: 'center', gap: 10 }}>
              <div className="themeToggle" role="group" aria-label="Preview theme">
                <button className={previewTheme === 'dark' ? 'on' : ''} onClick={() => setPreviewTheme('dark')}>Dark</button>
                <button className={previewTheme === 'light' ? 'on' : ''} onClick={() => setPreviewTheme('light')}>Light</button>
              </div>
              <span className="muted" style={{ fontSize: '0.8rem' }}>
                800 × 480 · the car follows its own day/night theme · drag to move, corner to resize
              </span>
            </div>
          </div>

          {/* property panel */}
          <aside className="dashEditorProps">
            {!selected ? <p className="muted" style={{ padding: 8 }}>Select a widget to edit its data &amp; style.</p> : (
              <PropertyPanel key={selected.id} w={selected} fields={fields} onChange={(p) => patch(selected.id, p)} onDelete={removeSelected} />
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

function PropertyPanel({ w, fields, onChange, onDelete }: { w: Widget; fields: FieldDef[]; onChange: (p: Partial<Widget>) => void; onDelete: () => void }) {
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
              if ((w.type === 'value' || w.type === 'delta') && fd.defaultFormat) (p as { format?: FormatId }).format = fd.defaultFormat;
              if ((w.type === 'bar' || w.type === 'gauge')) {
                if (fd.min != null) (p as { min?: number }).min = fd.min;
                if (fd.max != null) (p as { max?: number }).max = fd.max;
                if (fd.bidirectional != null) (p as { bidirectional?: boolean }).bidirectional = fd.bidirectional;
              }
              if ((w.type === 'delta' || w.type === 'bar') && fd.goodSign) (p as { goodSign?: GoodSign }).goodSign = fd.goodSign;
              // Enum/bitfield fields (e.g. VCU event mode) pre-fill the value→label
              // map so the widget shows "ENDUR" not 4; clears it for plain numbers.
              if (w.type === 'value') (p as { valueMap?: Record<string, string> }).valueMap = fd.enumMap;
            }
            onChange(p);
          }}>
            {Array.from(new Set(fields.map((f) => f.group))).map((g) => (
              <optgroup key={g} label={g}>
                {fields.filter((f) => f.group === g).map((f) => <option key={f.bind} value={f.bind}>{f.label}{f.unit ? ` (${f.unit})` : ''}</option>)}
              </optgroup>
            ))}
          </select>
        </Row>
      )}
      {(w.type === 'value' || w.type === 'gauge' || w.type === 'delta') && (
        <Row label="Format">
          <select value={(w as { format?: FormatId }).format ?? 'int'} onChange={(e) => onChange({ format: e.target.value as FormatId } as Partial<Widget>)}>
            {FORMATS.map((f) => <option key={f} value={f}>{f}</option>)}
          </select>
        </Row>
      )}
      {(w.type === 'value' || w.type === 'gauge' || w.type === 'delta') && (
        <Row label="Decimals">
          <select value={(w as { decimals?: number }).decimals ?? ''}
            onChange={(e) => onChange({ decimals: e.target.value === '' ? undefined : Number(e.target.value) } as Partial<Widget>)}>
            <option value="">Auto</option>
            <option value="0">0</option>
            <option value="1">1</option>
            <option value="2">2</option>
            <option value="3">3</option>
            <option value="4">4</option>
          </select>
        </Row>
      )}
      {(w.type === 'value' || w.type === 'bar' || w.type === 'gauge' || w.type === 'delta') && (
        <Row label="Label"><input value={(w as { label?: string }).label ?? ''} onChange={(e) => onChange({ label: e.target.value } as Partial<Widget>)} /></Row>
      )}
      {w.type === 'delta' && (
        <>
          <Row label="Green when">
            <select value={w.goodSign ?? 'negative'} onChange={(e) => onChange({ goodSign: e.target.value as GoodSign })}>
              <option value="negative">lower / negative</option>
              <option value="positive">higher / positive</option>
            </select>
          </Row>
          <label className="propRow"><span>Arrow ▲▼</span><input type="checkbox" checked={w.showArrow ?? false} onChange={(e) => onChange({ showArrow: e.target.checked })} /></label>
          <label className="propRow"><span>+/− sign</span><input type="checkbox" checked={w.showSign !== false} onChange={(e) => onChange({ showSign: e.target.checked })} /></label>
        </>
      )}
      {w.type === 'bar' && (
        <>
          <label className="propRow"><span>Sign colors</span><input type="checkbox" checked={w.signColored ?? false} onChange={(e) => onChange({ signColored: e.target.checked })} /></label>
          {w.signColored && (
            <Row label="Green when">
              <select value={w.goodSign ?? 'negative'} onChange={(e) => onChange({ goodSign: e.target.value as GoodSign })}>
                <option value="negative">lower / negative</option>
                <option value="positive">higher / positive</option>
              </select>
            </Row>
          )}
        </>
      )}
      {(w.type === 'bar' || w.type === 'gauge') && (
        <>
          <Row label="Min"><input type="number" value={(w as { min: number }).min} onChange={(e) => onChange({ min: Number(e.target.value) } as Partial<Widget>)} /></Row>
          <Row label="Max"><input type="number" value={(w as { max: number }).max} onChange={(e) => onChange({ max: Number(e.target.value) } as Partial<Widget>)} /></Row>
        </>
      )}
      {(w.type === 'text' || w.type === 'value' || w.type === 'delta') && (
        <Row label="Font size"><input type="number" value={w.fontSize} onChange={(e) => onChange({ fontSize: Number(e.target.value) } as Partial<Widget>)} /></Row>
      )}
      {(w.type === 'text' || w.type === 'value' || w.type === 'gauge') && (
        <ColorControl w={w} onChange={onChange} />
      )}
      {w.type === 'bar' && !w.signColored && (
        <Row label="Fill color"><input type="color" value={normalizeColor(w.color)} onChange={(e) => onChange({ color: e.target.value })} /></Row>
      )}
      {(w.type === 'text' || w.type === 'value' || w.type === 'delta') && (
        <Row label="Align">
          <select value={(w as { align?: Align }).align ?? 'center'} onChange={(e) => onChange({ align: e.target.value as Align } as Partial<Widget>)}>
            <option value="left">left</option><option value="center">center</option><option value="right">right</option>
          </select>
        </Row>
      )}
      {w.type === 'value' && (
        <ThresholdRules rules={w.thresholds ?? []} onChange={(r) => onChange({ thresholds: r })} />
      )}
      {w.type === 'value' && (
        <ValueLabels key={w.bind} map={w.valueMap} onChange={(m) => onChange({ valueMap: m })} />
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

function ThresholdRules({ rules, onChange }: { rules: ColorRule[]; onChange: (r: ColorRule[]) => void }) {
  const update = (i: number, p: Partial<ColorRule>) => onChange(rules.map((r, idx) => (idx === i ? { ...r, ...p } : r)));
  return (
    <div className="thresholdRules">
      <div className="propHead"><span className="muted" style={{ fontSize: '0.78rem', textTransform: 'uppercase', letterSpacing: 1 }}>Color rules</span>
        <button className="tool" onClick={() => onChange([...rules, { cmp: 'lt', value: 20, color: '#ff4d4f' }])}>+ Rule</button>
      </div>
      {rules.map((r, i) => (
        <div key={i} className="thresholdRule">
          <select value={r.cmp} onChange={(e) => update(i, { cmp: e.target.value as 'lt' | 'gt' })}>
            <option value="lt">&lt;</option><option value="gt">&gt;</option>
          </select>
          <input type="number" value={r.value} onChange={(e) => update(i, { value: Number(e.target.value) })} />
          <input type="color" value={r.color} onChange={(e) => update(i, { color: e.target.value })} />
          <button className="tool iconOnly" aria-label="Remove rule" onClick={() => onChange(rules.filter((_, idx) => idx !== i))}><Trash2 size={12} /></button>
        </div>
      ))}
      {!rules.length && <p className="muted" style={{ fontSize: '0.78rem', margin: '2px 0 0' }}>e.g. &lt; 20 → red for low SoC. Last matching rule wins.</p>}
    </div>
  );
}

// Value→label map editor for value widgets — turns an enum/bitfield number into
// its meaning (e.g. 4 → ENDUR). Binding an enum field (VCU event mode) pre-fills
// this; it's editable for any field. A value with no matching key shows numerically.
//
// Edits an ordered ROW LIST internally (not the value→label object directly) so
// "+ Label" always appends a fresh blank row, blank/half-typed rows persist while
// you fill them, and typing a key never reorders or overwrites another row. The
// object is rebuilt (deduped, blank keys dropped) only when emitting. Remounted
// per field via key={w.bind} at the call site, so an external prefill flows in.
function ValueLabels({ map, onChange }: { map?: Record<string, string>; onChange: (m: Record<string, string> | undefined) => void }) {
  const [rows, setRows] = useState<[string, string][]>(() => Object.entries(map ?? {}));
  const emit = (next: [string, string][]) => {
    setRows(next);
    const obj: Record<string, string> = {};
    for (const [k, v] of next) { const kk = k.trim(); if (kk !== '') obj[kk] = v; } // last wins on dup key
    onChange(Object.keys(obj).length ? obj : undefined);
  };
  return (
    <div className="thresholdRules">
      <div className="propHead"><span className="muted" style={{ fontSize: '0.78rem', textTransform: 'uppercase', letterSpacing: 1 }}>Value labels</span>
        <button className="tool" onClick={() => emit([...rows, ['', '']])}>+ Label</button>
      </div>
      {rows.map(([k, v], i) => (
        <div key={i} className="thresholdRule">
          <input type="number" value={k} placeholder="#" style={{ width: 56 }} aria-label="value"
            onChange={(e) => emit(rows.map((r, idx) => (idx === i ? [e.target.value, r[1]] : r)))} />
          <input type="text" value={v} placeholder="label" aria-label="label"
            onChange={(e) => emit(rows.map((r, idx) => (idx === i ? [r[0], e.target.value] : r)))} />
          <button className="tool iconOnly" aria-label="Remove label" onClick={() => emit(rows.filter((_, idx) => idx !== i))}><Trash2 size={12} /></button>
        </div>
      ))}
      {!rows.length && <p className="muted" style={{ fontSize: '0.78rem', margin: '2px 0 0' }}>Map a number to text, e.g. 4 → ENDUR (enums / bitfields).</p>}
    </div>
  );
}

// Color picker with "Auto (theme)" + optional per-mode (dark/light) overrides,
// so a widget can follow the theme or be pinned per mode if auto looks off.
function ColorControl({ w, onChange }: { w: { color?: string; colorDark?: string; colorLight?: string }; onChange: (p: Partial<Widget>) => void }) {
  const auto = !w.color || w.color === 'auto';
  const perMode = (label: string, val: string | undefined, key: 'colorDark' | 'colorLight') =>
    val ? (
      <span style={{ display: 'inline-flex', gap: 3, alignItems: 'center' }}>
        <input type="color" value={normalizeColor(val)} title={`${label} override`} onChange={(e) => onChange({ [key]: e.target.value } as Partial<Widget>)} />
        <button className="tool iconOnly" aria-label={`clear ${label}`} onClick={() => onChange({ [key]: undefined } as Partial<Widget>)}>×</button>
      </span>
    ) : (
      <button className="tool" onClick={() => onChange({ [key]: '#ffffff' } as Partial<Widget>)}>+ {label}</button>
    );
  return (
    <>
      <Row label="Color">
        <div style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
          <select value={auto ? 'auto' : 'custom'}
            onChange={(e) => onChange({ color: e.target.value === 'auto' ? 'auto' : (w.color && w.color !== 'auto' ? w.color : '#ffffff') } as Partial<Widget>)}>
            <option value="auto">Auto (theme)</option>
            <option value="custom">Custom</option>
          </select>
          {!auto && <input type="color" value={normalizeColor(w.color)} onChange={(e) => onChange({ color: e.target.value } as Partial<Widget>)} />}
        </div>
      </Row>
      <Row label="Per-mode">
        <div style={{ display: 'flex', gap: 10, alignItems: 'center', flexWrap: 'wrap' }}>
          {perMode('dark', w.colorDark, 'colorDark')}
          {perMode('light', w.colorLight, 'colorLight')}
        </div>
      </Row>
    </>
  );
}

function normalizeColor(c?: string): string {
  if (!c || c === 'gradient') return '#ffffff';
  return c.startsWith('#') ? c : '#ffffff';
}

export default DashLayoutEditor;
