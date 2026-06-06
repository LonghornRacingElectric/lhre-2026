// Shared registry of trackside sessions, persisted to localStorage so it is
// readable across same-origin pages in one browser. Trackside-live writes a
// record when the strategist starts a "New Session" (name, car, driver, event,
// venue, start time); the Log Sync page reads it back to auto-fill a CSV's
// annotation by matching the file's loggerd start timestamp into a session's
// time window. One laptop at the trackside is the assumed workflow.

export const SESSION_REGISTRY_KEY = 'trackside-sessions';
const MAX_SESSIONS = 60;

// If a session is never explicitly ended, assume at most this long when matching
// a CSV into its window — keeps a forgotten-open session from claiming logs hours
// later.
const OPEN_SESSION_MAX_MS = 6 * 60 * 60 * 1000; // 6h

// FSAE Electric event types (autofill into the logsync "session" field / tags).
export const EVENT_TYPES = [
  'Practice',
  'Acceleration',
  'Skidpad',
  'Autocross',
  'Endurance',
  'Efficiency',
  'Test',
  'Other',
] as const;
export type EventType = (typeof EVENT_TYPES)[number];

export interface TracksideSessionInfo {
  id: string;
  name: string;
  car: string; // "orion" | "angelique"
  driver: string;
  eventType: string;
  venue: string; // track / venue name
  startedAt: number; // epoch ms
  endedAt: number | null; // epoch ms, null while live
  laps: number;
}

export function loadSessionRegistry(): TracksideSessionInfo[] {
  if (typeof window === 'undefined') return [];
  try {
    const raw = localStorage.getItem(SESSION_REGISTRY_KEY);
    const parsed = raw ? (JSON.parse(raw) as unknown) : [];
    if (!Array.isArray(parsed)) return [];
    return parsed.filter((s): s is TracksideSessionInfo =>
      !!s && typeof s === 'object' && typeof (s as TracksideSessionInfo).id === 'string'
      && typeof (s as TracksideSessionInfo).startedAt === 'number');
  } catch {
    return [];
  }
}

function saveSessionRegistry(list: TracksideSessionInfo[]): void {
  if (typeof window === 'undefined') return;
  try {
    // Keep the most recent sessions (by start time), capped.
    const trimmed = [...list].sort((a, b) => b.startedAt - a.startedAt).slice(0, MAX_SESSIONS);
    localStorage.setItem(SESSION_REGISTRY_KEY, JSON.stringify(trimmed));
  } catch {
    // localStorage full / unavailable — non-fatal, the registry is best-effort.
  }
}

/** Insert or replace a session by id. */
export function upsertSession(info: TracksideSessionInfo): void {
  const list = loadSessionRegistry().filter((s) => s.id !== info.id);
  list.push(info);
  saveSessionRegistry(list);
}

/** Patch an existing session (e.g. bump endedAt / lap count as it runs). */
export function patchSession(id: string, patch: Partial<TracksideSessionInfo>): void {
  const list = loadSessionRegistry();
  const idx = list.findIndex((s) => s.id === id);
  if (idx === -1) return;
  list[idx] = { ...list[idx], ...patch };
  saveSessionRegistry(list);
}

/**
 * Find the trackside session whose time window contains the given loggerd
 * timestamp. A live (un-ended) session matches up to OPEN_SESSION_MAX_MS after
 * its start. If several match, the latest-starting one wins.
 */
export function findSessionForTimestamp(ms: number): TracksideSessionInfo | null {
  const matches = loadSessionRegistry().filter((s) => {
    const end = s.endedAt ?? s.startedAt + OPEN_SESSION_MAX_MS;
    return ms >= s.startedAt && ms <= end;
  });
  if (!matches.length) return null;
  return matches.sort((a, b) => b.startedAt - a.startedAt)[0];
}

/** Parse the loggerd start epoch (ms) from a CSV name like `orion_1717612345678.csv`. */
export function parseLogStartMs(fileName: string): number | null {
  const m = fileName.match(/_(\d{10,13})\./);
  if (!m) return null;
  const n = Number(m[1]);
  if (!Number.isFinite(n)) return null;
  // 10-digit values are seconds; 13-digit are ms.
  return m[1].length <= 10 ? n * 1000 : n;
}
