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
  // Full session record (synced + logged server-side, not just the summary):
  // the rich MoTeC-style metadata and the energy plan. Optional so older
  // records still validate.
  metadata?: Record<string, string>;
  plan?: { targetLaps: number; targetEnergyKwh: number; soeCutoffCellV: number };
  updatedAt?: number; // epoch ms of last edit, for last-writer-wins on merge
  dayId?: number; // linked drive_day.day_id (session ⇒ drive day); for event flags + laps
  setup?: Record<string, string | number | boolean>; // drive-day setup (conditions/tires/aero/alignment)
  // The clientId of the user who started this session (presence.clientId at
  // creation time). On rejoin, that user gets a one-click reclaim instead of
  // requesting + waiting on the current leader to grant.
  starterId?: string;
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

/** Insert or replace a session by id. Mirrors to the logsync server so other
 *  devices can match a CSV against it; localStorage stays as an offline cache. */
export function upsertSession(info: TracksideSessionInfo): void {
  const stamped = { ...info, updatedAt: Date.now() };
  const list = loadSessionRegistry().filter((s) => s.id !== info.id);
  list.push(stamped);
  saveSessionRegistry(list);
  void pushSessionToServer(stamped);
}

/** Patch an existing session (e.g. bump endedAt / lap count / metadata as it runs). */
export function patchSession(id: string, patch: Partial<TracksideSessionInfo>): void {
  const list = loadSessionRegistry();
  const idx = list.findIndex((s) => s.id === id);
  if (idx === -1) return;
  list[idx] = { ...list[idx], ...patch, updatedAt: Date.now() };
  saveSessionRegistry(list);
  void pushSessionToServer(list[idx]);
}

/** Reconcile local + server registries (last-writer-wins by updatedAt) and
 *  re-push any local records the server is missing or behind on — so every
 *  client converges on the full record and a record that failed to push earlier
 *  (offline) gets retried on the next load. Returns the merged list. */
export async function syncRegistryWithServer(): Promise<TracksideSessionInfo[]> {
  const local = loadSessionRegistry();
  const server = await fetchSessionsFromServer();
  const byId = new Map<string, TracksideSessionInfo>();
  for (const s of local) byId.set(s.id, s);
  const toPush: TracksideSessionInfo[] = [];
  for (const srv of server) {
    const loc = byId.get(srv.id);
    if (!loc || (srv.updatedAt ?? 0) >= (loc.updatedAt ?? 0)) byId.set(srv.id, srv); // server newer/equal → adopt
  }
  for (const loc of local) {
    const srv = server.find((s) => s.id === loc.id);
    if (!srv || (loc.updatedAt ?? 0) > (srv.updatedAt ?? 0)) toPush.push(loc); // local newer/missing → re-push
  }
  const merged = Array.from(byId.values());
  saveSessionRegistry(merged);
  for (const s of toPush) void pushSessionToServer(s);
  return merged;
}

// ---- server bridge (logsync worker, proxied via /api/logsync) --------------

/** Upsert the FULL session record to the logsync worker for cross-device match
 *  + durable server-side logging. Retries with backoff so a transient dropout
 *  doesn't lose the record (the "full, no partial" requirement); localStorage
 *  remains the offline cache. Stamps updatedAt for last-writer-wins on merge. */
export async function pushSessionToServer(info: TracksideSessionInfo): Promise<boolean> {
  if (typeof fetch === 'undefined') return false;
  const body = JSON.stringify({ ...info, updatedAt: info.updatedAt ?? Date.now() });
  const delays = [0, 1000, 3000, 8000]; // 4 attempts
  for (let i = 0; i < delays.length; i++) {
    if (delays[i]) await new Promise((r) => setTimeout(r, delays[i]));
    try {
      const res = await fetch('/api/logsync/sessions', {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body,
      });
      if (res.ok) return true;
    } catch {
      // network error — retry
    }
  }
  return false; // exhausted retries; localStorage copy still holds it for re-push next load
}

/** Fetch all server-side sessions (empty list if the worker is unreachable). */
export async function fetchSessionsFromServer(): Promise<TracksideSessionInfo[]> {
  if (typeof fetch === 'undefined') return [];
  try {
    const res = await fetch('/api/logsync/sessions');
    if (!res.ok) return [];
    const map = (await res.json()) as Record<string, TracksideSessionInfo>;
    return Object.values(map).filter((s) => s && typeof s.id === 'string' && typeof s.startedAt === 'number');
  } catch {
    return [];
  }
}

/** Match a loggerd timestamp into one of the given sessions (latest start wins). */
export function findSessionForTimestampIn(list: TracksideSessionInfo[], ms: number): TracksideSessionInfo | null {
  const matches = list.filter((s) => {
    const end = s.endedAt ?? s.startedAt + OPEN_SESSION_MAX_MS;
    return ms >= s.startedAt && ms <= end;
  });
  if (!matches.length) return null;
  return matches.sort((a, b) => b.startedAt - a.startedAt)[0];
}

/**
 * Find the trackside session whose time window contains the given loggerd
 * timestamp. A live (un-ended) session matches up to OPEN_SESSION_MAX_MS after
 * its start. If several match, the latest-starting one wins.
 */
export function findSessionForTimestamp(ms: number): TracksideSessionInfo | null {
  return findSessionForTimestampIn(loadSessionRegistry(), ms);
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
