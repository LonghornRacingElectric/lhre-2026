'use client';

// Renders a single DRIVER MESSAGE full-screen on the dash's native 800x480,
// scaled onto a stage. INVERTED for cockpit legibility: the message's color
// fills the ENTIRE screen and the text/icon are "cut out" in a high-contrast
// ink (light text on dark colors, dark text on light ones), so the driver reads
// it at a glance. Theme-aware fallback when no message is showing. No icon-lib
// dependency — glyphs are plain text — so this file ships verbatim into the
// on-car dashd CRA frontend.

import React from 'react';
import { CARD_THEMES, type CardTheme } from '@/lib/dash/dashLayout';
import { MESSAGE_ICON_GLYPH, messageColor, type DashMessage } from '@/lib/dash/dashMessages';

export const MSG_CARD_W = 800;
export const MSG_CARD_H = 480;

// Relative luminance of a #rrggbb color (sRGB-weighted), 0 (black) … 1 (white).
function luminance(hex: string): number {
  const m = /^#?([0-9a-fA-F]{6})$/.exec(hex);
  if (!m) return 0.5;
  const n = parseInt(m[1], 16);
  const r = (n >> 16) & 255, g = (n >> 8) & 255, b = n & 255;
  return (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255;
}

export function MessageCardRenderer({ message, theme = 'dark', scale = 1 }: {
  message: DashMessage | null;
  theme?: CardTheme;
  scale?: number;
}) {
  const pal = CARD_THEMES[theme];
  const accent = message ? messageColor(message, theme) : pal.cardBg;
  // Cut-out ink: pick black or white for maximum contrast against the colored
  // field so the message stays readable whatever color the strategist chose.
  const ink = luminance(accent) > 0.55 ? '#0b0c0e' : '#ffffff';
  const glyph = message ? MESSAGE_ICON_GLYPH[message.icon] : '';
  // Big text auto-shrinks for longer lines so it always fits one row.
  const len = message?.text.length ?? 0;
  const bigSize = len <= 10 ? 140 : len <= 16 ? 108 : len <= 24 ? 80 : 56;
  return (
    <div style={{ width: MSG_CARD_W * scale, height: MSG_CARD_H * scale, position: 'relative', flex: 'none' }}>
      <div style={{
        width: MSG_CARD_W, height: MSG_CARD_H, transform: `scale(${scale})`, transformOrigin: 'top left',
        position: 'absolute', top: 0, left: 0, background: message ? accent : pal.cardBg, overflow: 'hidden',
        fontFamily: 'system-ui, sans-serif', display: 'flex', flexDirection: 'column',
        alignItems: 'center', justifyContent: 'center', gap: 14, padding: 44, boxSizing: 'border-box',
      }}>
        {!message ? (
          <span style={{ color: pal.muted, fontSize: 32 }}>No message</span>
        ) : (
          <>
            {glyph && <span style={{ fontSize: 104, lineHeight: 1, color: ink }}>{glyph}</span>}
            <span style={{
              color: ink, fontSize: bigSize, fontWeight: 900, lineHeight: 1.0,
              textAlign: 'center', letterSpacing: 1, textTransform: 'uppercase',
            }}>{message.text}</span>
            {message.subtext && (
              <span style={{ color: ink, fontSize: 38, fontWeight: 600, textAlign: 'center', opacity: 0.82 }}>
                {message.subtext}
              </span>
            )}
          </>
        )}
      </div>
    </div>
  );
}

export default MessageCardRenderer;
