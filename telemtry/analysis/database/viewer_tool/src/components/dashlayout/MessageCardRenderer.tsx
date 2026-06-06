'use client';

// Renders a single DRIVER MESSAGE full-screen on the dash's native 800x480,
// scaled onto a stage. Theme-aware like LapCardRenderer (the car passes its
// effective day/night theme; the editor previews both). No icon-library
// dependency — glyphs are plain text — so this file ships verbatim into the
// on-car dashd CRA frontend.

import React from 'react';
import { CARD_THEMES, type CardTheme } from '@/lib/dash/dashLayout';
import { MESSAGE_ICON_GLYPH, messageColor, type DashMessage } from '@/lib/dash/dashMessages';

export const MSG_CARD_W = 800;
export const MSG_CARD_H = 480;

export function MessageCardRenderer({ message, theme = 'dark', scale = 1 }: {
  message: DashMessage | null;
  theme?: CardTheme;
  scale?: number;
}) {
  const pal = CARD_THEMES[theme];
  const accent = message ? messageColor(message, theme) : pal.fg;
  const glyph = message ? MESSAGE_ICON_GLYPH[message.icon] : '';
  // Big text auto-shrinks for longer lines so it always fits one row.
  const len = message?.text.length ?? 0;
  const bigSize = len <= 10 ? 132 : len <= 16 ? 104 : len <= 24 ? 76 : 56;
  return (
    <div style={{ width: MSG_CARD_W * scale, height: MSG_CARD_H * scale, position: 'relative', flex: 'none' }}>
      <div style={{
        width: MSG_CARD_W, height: MSG_CARD_H, transform: `scale(${scale})`, transformOrigin: 'top left',
        position: 'absolute', top: 0, left: 0, background: pal.cardBg, overflow: 'hidden',
        fontFamily: 'system-ui, sans-serif', display: 'flex', flexDirection: 'column',
        alignItems: 'center', justifyContent: 'center', gap: 18, padding: 40, boxSizing: 'border-box',
        // A bold accent bar top + bottom frames the message and reads at a glance.
        borderTop: `12px solid ${accent}`, borderBottom: `12px solid ${accent}`,
      }}>
        {!message ? (
          <span style={{ color: pal.muted, fontSize: 32 }}>No message</span>
        ) : (
          <>
            {glyph && <span style={{ fontSize: 96, lineHeight: 1, color: accent }}>{glyph}</span>}
            <span style={{
              color: accent, fontSize: bigSize, fontWeight: 800, lineHeight: 1.02,
              textAlign: 'center', letterSpacing: 1, textTransform: 'uppercase',
            }}>{message.text}</span>
            {message.subtext && (
              <span style={{ color: pal.fg, fontSize: 38, fontWeight: 500, textAlign: 'center', opacity: 0.85 }}>
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
