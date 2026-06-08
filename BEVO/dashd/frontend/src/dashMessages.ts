// Shared schema for website-authored DRIVER MESSAGES — short, full-screen nudges
// the strategist fires at the car ("USE LESS ENERGY", "BOX THIS LAP", …).
//
// Pipeline mirrors the lap-card layout (see dashLayout.ts):
//   editor → server library (/api/dash/messages) → retained active set on
//   `lhre/dash/messages` → a non-retained trigger on `lhre/dash/message` that
//   the on-car dash renders with the SAME MessageCardRenderer.
// Keep this file in sync with the dashd copy.

import { CARD_THEMES, resolveColor, type CardTheme } from './dashLayout';

export const DASH_MESSAGE_VERSION = 1;

// How many messages can be "active" (loaded on the car / shown as quick-send
// buttons) at once. The trackside button row stays scannable and the retained
// set the car holds is bounded. The library itself is unlimited.
export const MAX_ACTIVE_MESSAGES = 8;

// Icons are rendered as plain glyphs so the renderer has NO icon-library
// dependency (it ships verbatim into the car's CRA frontend). Editor/buttons
// reuse the same map.
export type MessageIcon = 'none' | 'down' | 'up' | 'bolt' | 'flag' | 'warning' | 'check';
export const MESSAGE_ICON_GLYPH: Record<MessageIcon, string> = {
  none: '', down: '▼', up: '▲', bolt: '⚡', flag: '⚑', warning: '⚠', check: '✓',
};
export const MESSAGE_ICONS: MessageIcon[] = ['none', 'down', 'up', 'bolt', 'flag', 'warning', 'check'];

export interface DashMessage {
  id: string;
  label: string;            // short button caption trackside ("Use less")
  text: string;             // big line shown to the driver ("USE LESS ENERGY")
  subtext?: string;         // optional second line ("-2 kW target")
  icon: MessageIcon;
  color: string;            // hex accent, or "auto" to follow the dash theme fg
  colorDark?: string;       // optional explicit override in dark mode
  colorLight?: string;      // optional explicit override in light mode
  durationS: number;        // seconds on screen; 0 = sticky until cleared/replaced
}

export interface MessageLibrary {
  items: DashMessage[];
  activeIds: string[];      // ordered subset shown as quick-send buttons (≤ cap)
}

// What the strategist sends on a button press. Self-contained (carries the full
// message) so a freshly-booted dash can render it even before the retained set
// arrives. `at` makes each press a distinct event (re-fires the same message).
export interface MessageTrigger {
  at: number;
  clear?: boolean;          // true → dismiss whatever is on screen
  durationS?: number;       // overrides msg.durationS for this fire
  msg?: DashMessage;
}

export function genMessageId(): string {
  return `msg-${Date.now().toString(36)}-${Math.floor(Math.random() * 1e6).toString(36)}`;
}

// Resolve a message's accent color for the active theme (same precedence as
// lap-card widgets: per-mode override → explicit color → theme foreground).
export function messageColor(m: DashMessage, theme: CardTheme): string {
  return resolveColor(m, theme, CARD_THEMES[theme].fg);
}

export function newMessage(): DashMessage {
  return {
    id: genMessageId(), label: 'New message', text: 'MESSAGE',
    icon: 'none', color: 'auto', durationS: 6,
  };
}

// Starter set the strategist can tweak. Covers the common endurance nudges.
export function defaultMessages(): DashMessage[] {
  const mk = (label: string, text: string, icon: MessageIcon, color: string, durationS = 6, subtext?: string): DashMessage =>
    ({ id: genMessageId(), label, text, subtext, icon, color, durationS });
  return [
    mk('Use less', 'USE LESS ENERGY', 'down', '#5cb87a', 6, 'Ease off the throttle'),
    mk('Use more', 'USE MORE ENERGY', 'up', '#6ea8fe', 6, 'You have margin'),
    mk('Push', 'PUSH', 'bolt', '#d97757', 5),
    mk('Hold', 'HOLD POSITION', 'flag', '#f2c14e', 0),
    mk('Box this lap', 'BOX THIS LAP', 'warning', '#ff8d85', 0),
    mk('Box now', 'BOX NOW', 'warning', '#ff4d4f', 0),
    mk('OK / copy', 'OK', 'check', '#5cb87a', 4),
  ];
}

export function defaultMessageLibrary(): MessageLibrary {
  const items = defaultMessages();
  return { items, activeIds: items.slice(0, MAX_ACTIVE_MESSAGES).map((m) => m.id) };
}

// Lenient validation: drop malformed entries, clamp the active set to the cap.
export function validateMessage(raw: unknown): DashMessage | null {
  if (!raw || typeof raw !== 'object') return null;
  const o = raw as Partial<DashMessage>;
  if (typeof o.text !== 'string') return null;
  const icon = (MESSAGE_ICONS as string[]).includes(o.icon as string) ? (o.icon as MessageIcon) : 'none';
  const dur = typeof o.durationS === 'number' && o.durationS >= 0 ? o.durationS : 6;
  return {
    id: typeof o.id === 'string' ? o.id : genMessageId(),
    label: typeof o.label === 'string' && o.label ? o.label : (o.text || 'Message'),
    text: o.text,
    subtext: typeof o.subtext === 'string' ? o.subtext : undefined,
    icon,
    color: typeof o.color === 'string' ? o.color : 'auto',
    colorDark: typeof o.colorDark === 'string' ? o.colorDark : undefined,
    colorLight: typeof o.colorLight === 'string' ? o.colorLight : undefined,
    durationS: dur,
  };
}

export function validateMessageLibrary(raw: unknown): MessageLibrary | null {
  if (!raw || typeof raw !== 'object') return null;
  const o = raw as Partial<MessageLibrary>;
  if (!Array.isArray(o.items)) return null;
  const items = o.items.map(validateMessage).filter(Boolean) as DashMessage[];
  const ids = new Set(items.map((m) => m.id));
  const activeIds = (Array.isArray(o.activeIds) ? o.activeIds : [])
    .filter((id): id is string => typeof id === 'string' && ids.has(id))
    .slice(0, MAX_ACTIVE_MESSAGES);
  return { items, activeIds };
}

// The retained set the car holds — just the active messages, in order.
export function activeMessages(lib: MessageLibrary): DashMessage[] {
  const byId = new Map(lib.items.map((m) => [m.id, m]));
  return lib.activeIds.map((id) => byId.get(id)).filter(Boolean) as DashMessage[];
}
