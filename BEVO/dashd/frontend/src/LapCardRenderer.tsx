// SHARED renderer — source of truth:
// telemtry/analysis/database/viewer_tool/src/components/dashlayout/LapCardRenderer.tsx

// Renders a LapCardLayout at the dash's native 800x480 onto a scaled stage.
// Theme-aware: pass theme='dark'|'light' and colors/surfaces follow it (the car
// passes the dash's effective theme; the editor previews both). Widget colors
// default to "auto" (theme foreground) with optional per-mode overrides. The
// on-car dashd frontend uses an identical copy of this renderer.

import React from 'react';
import {
  LAP_CARD_W, LAP_CARD_H, formatValue, getByPath, deltaColor, valueColor,
  resolveColor, resolveBackground, CARD_THEMES,
  type LapCardLayout, type Widget, type CardTheme, type ThemePalette,
} from './dashLayout';

function MiniRadialGauge({ value, min, max, label, color = 'gradient', mode = 'standard', valueText, pal }: {
  value: number | null; min: number; max: number; label?: string; color?: string;
  mode?: 'standard' | 'bidirectional'; valueText: string; pal: ThemePalette;
}) {
  const size = 160, stroke = 14, pad = 6;
  const svg = size + pad * 2;
  const cx = svg / 2, cy = svg / 2, r = (size - stroke) / 2;
  const start = (3 * Math.PI) / 4, end = (9 * Math.PI) / 4, total = end - start;
  const x1 = cx + r * Math.cos(start), y1 = cy + r * Math.sin(start);
  const x2 = cx + r * Math.cos(end), y2 = cy + r * Math.sin(end);
  const d = `M ${x1} ${y1} A ${r} ${r} 0 1 1 ${x2} ${y2}`;
  const circ = r * total;
  const v = value ?? min;
  const pct = Math.min(1, Math.max(0, (Math.min(Math.max(v, min), max) - min) / (max - min)));
  const zeroPct = (0 - min) / (max - min);
  let dash = `${circ} ${circ}`, off = circ - pct * circ, strokeColor = color === 'gradient' ? 'url(#lg)' : color;
  if (mode === 'bidirectional') {
    const startPct = Math.min(zeroPct, pct), fillPct = Math.abs(pct - zeroPct);
    const sg = circ * startPct, fl = circ * fillPct;
    dash = `0 ${sg} ${fl} ${circ - sg - fl}`; off = 0;
    strokeColor = v < 0 ? '#2ee06a' : '#d97757';
  }
  return (
    <svg width="100%" height="100%" viewBox={`0 0 ${svg} ${svg}`} preserveAspectRatio="xMidYMid meet">
      <path d={d} fill="none" stroke={pal.track} strokeWidth={stroke} strokeLinecap="round" />
      <path d={d} fill="none" stroke={strokeColor} strokeWidth={stroke} strokeDasharray={dash} strokeDashoffset={off} strokeLinecap="butt" />
      {color === 'gradient' && mode !== 'bidirectional' && (
        <defs><linearGradient id="lg" x1="0%" y1="0%" x2="100%" y2="0%">
          <stop offset="0%" stopColor="#00ff00" /><stop offset="50%" stopColor="#ffff00" /><stop offset="100%" stopColor="#ff0000" />
        </linearGradient></defs>
      )}
      <text x={cx} y={cy} fill={pal.fg} fontSize="34" fontWeight="bold" textAnchor="middle" dominantBaseline="central">{valueText}</text>
      {label && <text x={cx} y={cy + 34} fill={pal.muted} fontSize="13" letterSpacing="2" textAnchor="middle">{label}</text>}
    </svg>
  );
}

function WidgetView({ w, data, theme, pal }: { w: Widget; data: unknown; theme: CardTheme; pal: ThemePalette }) {
  if (w.type === 'text') {
    return (
      <div style={{ width: '100%', height: '100%', display: 'flex', alignItems: 'center',
        justifyContent: w.align === 'left' ? 'flex-start' : w.align === 'right' ? 'flex-end' : 'center',
        color: resolveColor(w, theme, pal.fg), fontSize: w.fontSize, fontWeight: w.bold ? 800 : 400,
        letterSpacing: w.letterSpacing ?? 0, lineHeight: 1, textAlign: w.align ?? 'center', overflow: 'hidden' }}>
        {w.text}
      </div>
    );
  }
  if (w.type === 'value') {
    const val = getByPath(data, w.bind);
    const color = valueColor(val, resolveColor(w, theme, pal.fg), w.thresholds);
    return (
      <div style={{ width: '100%', height: '100%', display: 'flex', flexDirection: 'column',
        alignItems: w.align === 'left' ? 'flex-start' : w.align === 'right' ? 'flex-end' : 'center',
        justifyContent: 'center', lineHeight: 1, overflow: 'hidden' }}>
        {w.label && <span style={{ color: w.labelColor ?? pal.muted, fontSize: Math.max(11, w.fontSize * 0.28), letterSpacing: 3, marginBottom: 4 }}>{w.label}</span>}
        <span style={{ color, fontSize: w.fontSize, fontWeight: w.bold ? 800 : 500 }}>{formatValue(val, w.format)}</span>
      </div>
    );
  }
  if (w.type === 'bar') {
    const val = getByPath(data, w.bind) ?? w.min;
    const span = w.max - w.min || 1;
    const pct = Math.min(1, Math.max(0, (val - w.min) / span));
    const zeroPct = Math.min(1, Math.max(0, (0 - w.min) / span));
    const base = resolveColor(w, theme, '#d97757');
    const fill = w.signColored ? deltaColor(val, w.goodSign ?? 'negative') : (w.bidirectional ? (val >= 0 ? base : '#2ee06a') : base);
    return (
      <div style={{ width: '100%', height: '100%', display: 'flex', flexDirection: 'column', justifyContent: 'center', gap: 4 }}>
        {w.label && <span style={{ color: pal.muted, fontSize: 13, letterSpacing: 2 }}>{w.label}</span>}
        <div style={{ position: 'relative', width: '100%', flex: 1, background: w.bg ?? pal.track, borderRadius: 6, overflow: 'hidden' }}>
          {w.bidirectional ? (
            <div style={{ position: 'absolute', top: 0, bottom: 0,
              left: `${Math.min(zeroPct, pct) * 100}%`, width: `${Math.abs(pct - zeroPct) * 100}%`, background: fill }} />
          ) : (
            <div style={{ position: 'absolute', top: 0, bottom: 0, left: 0, width: `${pct * 100}%`, background: fill }} />
          )}
        </div>
      </div>
    );
  }
  if (w.type === 'delta') {
    const val = getByPath(data, w.bind);
    const color = deltaColor(val, w.goodSign ?? 'negative', w.goodColor, w.badColor, w.zeroColor ?? pal.muted);
    const mag = formatValue(val, w.format);
    const signed = val != null && val > 0 && w.showSign !== false ? `+${mag}` : mag;
    const arrow = w.showArrow && val != null && val !== 0 ? (val > 0 ? '▲ ' : '▼ ') : '';
    return (
      <div style={{ width: '100%', height: '100%', display: 'flex', flexDirection: 'column',
        alignItems: w.align === 'left' ? 'flex-start' : w.align === 'right' ? 'flex-end' : 'center',
        justifyContent: 'center', lineHeight: 1, overflow: 'hidden' }}>
        {w.label && <span style={{ color: w.labelColor ?? pal.muted, fontSize: Math.max(11, w.fontSize * 0.28), letterSpacing: 3, marginBottom: 4 }}>{w.label}</span>}
        <span style={{ color, fontSize: w.fontSize, fontWeight: w.bold ? 800 : 600 }}>{arrow}{signed}</span>
      </div>
    );
  }
  // gauge
  const val = getByPath(data, w.bind);
  return (
    <div style={{ width: '100%', height: '100%' }}>
      <MiniRadialGauge value={val} min={w.min} max={w.max} label={w.label} color={resolveColor(w, theme, w.color ?? 'gradient')}
        mode={w.mode} valueText={formatValue(val, w.format ?? 'int')} pal={pal} />
    </div>
  );
}

export function LapCardRenderer({ layout, data, scale = 1, theme = 'dark', onWidgetMouseDown, selectedId, editable }: {
  layout: LapCardLayout;
  data: unknown;
  scale?: number;
  theme?: CardTheme;
  selectedId?: string | null;
  editable?: boolean;
  onWidgetMouseDown?: (id: string, e: React.MouseEvent) => void;
}) {
  const pal = CARD_THEMES[theme];
  const outline = theme === 'light' ? 'rgba(0,0,0,0.25)' : 'rgba(255,255,255,0.25)';
  return (
    <div style={{ width: LAP_CARD_W * scale, height: LAP_CARD_H * scale, position: 'relative', flex: 'none' }}>
      <div style={{ width: LAP_CARD_W, height: LAP_CARD_H, transform: `scale(${scale})`, transformOrigin: 'top left',
        position: 'absolute', top: 0, left: 0, background: resolveBackground(layout.background, theme), overflow: 'hidden',
        fontFamily: 'system-ui, sans-serif' }}>
        {layout.widgets.map((w) => (
          <div key={w.id}
            onMouseDown={editable ? (e) => onWidgetMouseDown?.(w.id, e) : undefined}
            style={{ position: 'absolute', left: w.x, top: w.y, width: w.w, height: w.h,
              cursor: editable ? 'move' : 'default',
              outline: editable ? (selectedId === w.id ? '2px solid #4ea1ff' : `1px dashed ${outline}`) : 'none' }}>
            <WidgetView w={w} data={data} theme={theme} pal={pal} />
          </div>
        ))}
      </div>
    </div>
  );
}

export default LapCardRenderer;
