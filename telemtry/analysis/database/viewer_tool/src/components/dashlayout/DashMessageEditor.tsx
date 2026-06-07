'use client';

// Editor for the driver-message palette. Author short full-screen nudges
// ("USE LESS ENERGY"), preview them in dark/light, mark up to MAX_ACTIVE_MESSAGES
// as the quick-send set shown on the Dash tab. Library is shared server-side via
// useMessageLibrary (passed in from TracksideApp so the buttons stay in sync).

import React, { useState } from 'react';
import { X, Plus, Copy, Trash2, RefreshCw, Cloud, Star, Send } from 'lucide-react';
import MessageCardRenderer from './MessageCardRenderer';
import {
  MESSAGE_ICONS, MESSAGE_ICON_GLYPH, MAX_ACTIVE_MESSAGES,
  type DashMessage, type MessageIcon,
} from '@/lib/dash/dashMessages';
import type { MessageLibraryApi } from '@/lib/dash/useMessageLibrary';

const SCALE = 0.58; // 800 -> 464 preview

export function DashMessageEditor({ api, onClose, onSendTest, sendStatus, canSend }: {
  api: MessageLibraryApi;
  onClose: () => void;
  onSendTest: (m: DashMessage) => void;
  sendStatus?: string;
  canSend: boolean;
}) {
  const { lib, syncMsg, pullFromServer, addMessage, duplicateMessage, updateMessage, removeMessage,
    toggleActive, isActive, atActiveCap } = api;
  const [selectedId, setSelectedId] = useState<string | null>(lib.items[0]?.id ?? null);
  const [previewTheme, setPreviewTheme] = useState<'dark' | 'light'>('dark');

  const selected = lib.items.find((m) => m.id === selectedId) ?? lib.items[0] ?? null;
  const change = (p: Partial<DashMessage>) => { if (selected) updateMessage(selected.id, p); };

  const remove = (id: string) => {
    const m = lib.items.find((x) => x.id === id);
    if (m && !window.confirm(`Delete message “${m.label}”?`)) return;
    removeMessage(id);
    if (selectedId === id) setSelectedId(lib.items.find((x) => x.id !== id)?.id ?? null);
  };

  return (
    <div className="modalOverlay" onMouseDown={onClose}>
      <div className="dashEditor" onMouseDown={(e) => e.stopPropagation()}>
        <div className="dashEditorHead">
          <div className="dashEditorTitle">
            <strong>Driver messages</strong>
            <button className="tool" onClick={() => setSelectedId(addMessage())} title="New message"><Plus size={14} /> New</button>
            {selected && <button className="tool" onClick={() => duplicateMessage(selected.id)} title="Duplicate"><Copy size={14} /></button>}
          </div>
          <div className="dashEditorActions">
            <span className="syncChip" title="Saved messages are shared with all clients"><Cloud size={13} /> {syncMsg}</span>
            <button className="tool iconOnly" title="Refresh from server" onClick={() => pullFromServer()}><RefreshCw size={14} /></button>
            <span className="muted" style={{ fontSize: '0.8rem', whiteSpace: 'nowrap' }}>
              {lib.activeIds.length}/{MAX_ACTIVE_MESSAGES} active on car
            </span>
            <button className="tool iconOnly" onClick={onClose} aria-label="Close"><X size={16} /></button>
          </div>
        </div>
        {sendStatus ? <div className="dashEditorStatus">{sendStatus}</div> : null}

        <div className="dashEditorBody">
          {/* message list with active-star toggles */}
          <aside className="dashEditorRail">
            <div className="widgetList">
              {lib.items.map((m) => {
                const active = isActive(m.id);
                const blocked = !active && atActiveCap;
                return (
                  <div key={m.id} className={`msgItem${m.id === selectedId ? ' sel' : ''}`}>
                    <button className="msgItemMain" onClick={() => setSelectedId(m.id)}>
                      <span className="msgItemGlyph">{MESSAGE_ICON_GLYPH[m.icon] || '•'}</span>
                      <span className="msgItemLabel">{m.label}</span>
                    </button>
                    <button
                      className={`starToggle${active ? ' on' : ''}`}
                      title={active ? 'Active — shown as a quick-send button' : blocked ? `Active set full (${MAX_ACTIVE_MESSAGES})` : 'Add to quick-send buttons'}
                      disabled={blocked}
                      onClick={() => toggleActive(m.id)}>
                      <Star size={14} fill={active ? 'currentColor' : 'none'} />
                    </button>
                  </div>
                );
              })}
              {!lib.items.length && <p className="muted" style={{ padding: 8 }}>Add a message to begin.</p>}
            </div>
          </aside>

          {/* preview */}
          <div className="dashEditorStage" style={{ background: previewTheme === 'light' ? '#d9d6d2' : '#0b0c0e' }}>
            <MessageCardRenderer message={selected} theme={previewTheme} scale={SCALE} />
            <div style={{ marginTop: 8, display: 'flex', alignItems: 'center', gap: 10, flexWrap: 'wrap' }}>
              <div className="themeToggle" role="group" aria-label="Preview theme">
                <button className={previewTheme === 'dark' ? 'on' : ''} onClick={() => setPreviewTheme('dark')}>Dark</button>
                <button className={previewTheme === 'light' ? 'on' : ''} onClick={() => setPreviewTheme('light')}>Light</button>
              </div>
              {selected && (
                <button className="tool" disabled={!canSend} title={canSend ? 'Send this message to the car now' : 'Connect to the dash first'}
                  onClick={() => onSendTest(selected)}><Send size={13} /> Test send</button>
              )}
              <span className="muted" style={{ fontSize: '0.8rem' }}>800 × 480 · the car follows its own day/night theme</span>
            </div>
          </div>

          {/* property form */}
          <aside className="dashEditorProps">
            {!selected ? <p className="muted" style={{ padding: 8 }}>Select a message to edit it.</p> : (
              <div className="propPanel">
                <div className="propHead"><strong>message</strong>
                  <button className="tool dangerTool" onClick={() => remove(selected.id)}><Trash2 size={13} /> Delete</button>
                </div>
                <label className="propRow"><span>Button label</span>
                  <input value={selected.label} onChange={(e) => change({ label: e.target.value })} /></label>
                <label className="propRow"><span>Display text</span>
                  <input value={selected.text} onChange={(e) => change({ text: e.target.value })} /></label>
                <label className="propRow"><span>Sub-text</span>
                  <input value={selected.subtext ?? ''} placeholder="(optional)" onChange={(e) => change({ subtext: e.target.value || undefined })} /></label>
                <label className="propRow"><span>Icon</span>
                  <select value={selected.icon} onChange={(e) => change({ icon: e.target.value as MessageIcon })}>
                    {MESSAGE_ICONS.map((ic) => <option key={ic} value={ic}>{ic === 'none' ? 'none' : `${MESSAGE_ICON_GLYPH[ic]}  ${ic}`}</option>)}
                  </select>
                </label>
                <ColorControl m={selected} onChange={change} />
                <label className="propRow"><span>Duration (s)</span>
                  <input type="number" min={0} value={selected.durationS}
                    onChange={(e) => change({ durationS: Math.max(0, Number(e.target.value)) })} />
                </label>
                <p className="muted" style={{ fontSize: '0.78rem', margin: '2px 0 0' }}>0 = stays until you clear it or send another.</p>
              </div>
            )}
          </aside>
        </div>
      </div>
    </div>
  );
}

function normalizeColor(c?: string): string {
  if (!c || c === 'auto') return '#ffffff';
  return c.startsWith('#') ? c : '#ffffff';
}

// Same Auto/Custom + per-mode override control as the lap-card editor.
function ColorControl({ m, onChange }: { m: DashMessage; onChange: (p: Partial<DashMessage>) => void }) {
  const auto = !m.color || m.color === 'auto';
  const perMode = (label: string, val: string | undefined, key: 'colorDark' | 'colorLight') =>
    val ? (
      <span style={{ display: 'inline-flex', gap: 3, alignItems: 'center' }}>
        <input type="color" value={normalizeColor(val)} title={`${label} override`} onChange={(e) => onChange({ [key]: e.target.value })} />
        <button className="tool iconOnly" aria-label={`clear ${label}`} onClick={() => onChange({ [key]: undefined })}>×</button>
      </span>
    ) : (
      <button className="tool" onClick={() => onChange({ [key]: '#ffffff' })}>+ {label}</button>
    );
  return (
    <>
      <label className="propRow"><span>Color</span>
        <div style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
          <select value={auto ? 'auto' : 'custom'}
            onChange={(e) => onChange({ color: e.target.value === 'auto' ? 'auto' : (m.color && m.color !== 'auto' ? m.color : '#ffffff') })}>
            <option value="auto">Auto (theme)</option>
            <option value="custom">Custom</option>
          </select>
          {!auto && <input type="color" value={normalizeColor(m.color)} onChange={(e) => onChange({ color: e.target.value })} />}
        </div>
      </label>
      <label className="propRow"><span>Per-mode</span>
        <div style={{ display: 'flex', gap: 10, alignItems: 'center', flexWrap: 'wrap' }}>
          {perMode('dark', m.colorDark, 'colorDark')}
          {perMode('light', m.colorLight, 'colorLight')}
        </div>
      </label>
    </>
  );
}

export default DashMessageEditor;
