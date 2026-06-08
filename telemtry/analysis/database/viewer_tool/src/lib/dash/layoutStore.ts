// Server bridge for the shared dash-layout library (/api/dash/layouts). Lets
// every client see the same saved layouts. localStorage stays as an offline
// cache + instant initial render; the server copy is the shared source.
//
// Layouts are namespaced per screen via `?screen=`. The default (`lapCard`)
// maps to the original library.json so existing saved lap cards are untouched;
// other screens (e.g. `park`) get their own file server-side.

import { validateLapCardLayout, type LapCardLayout } from './dashLayout';

export interface ServerLayouts { items: LapCardLayout[]; savedAt: number | null; }
// Result of a save: ok = written (savedAt is the new version); conflict = the
// server moved on since `baseSavedAt` (its current items/savedAt are returned so
// the caller can merge); neither = offline/unreachable.
export interface SaveResult { ok: boolean; conflict?: boolean; items?: LapCardLayout[]; savedAt?: number | null; }

export async function fetchServerLayouts(screen = 'lapCard'): Promise<ServerLayouts | null> {
  if (typeof fetch === 'undefined') return null;
  try {
    const res = await fetch(`/api/dash/layouts?screen=${encodeURIComponent(screen)}`);
    if (!res.ok) return null;
    const body = (await res.json()) as { items?: unknown[]; savedAt?: number };
    const items = (body.items ?? []).map((i) => validateLapCardLayout(i)).filter(Boolean) as LapCardLayout[];
    return { items, savedAt: typeof body.savedAt === 'number' ? body.savedAt : null };
  } catch {
    return null; // offline / unreachable — caller keeps localStorage copy
  }
}

export async function saveServerLayouts(screen: string, items: LapCardLayout[], baseSavedAt?: number | null): Promise<SaveResult> {
  if (typeof fetch === 'undefined') return { ok: false };
  try {
    const res = await fetch(`/api/dash/layouts?screen=${encodeURIComponent(screen)}`, {
      method: 'PUT',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ items, baseSavedAt }),
    });
    const body = (await res.json().catch(() => ({}))) as { ok?: boolean; conflict?: boolean; items?: unknown[]; savedAt?: number };
    const savedAt = typeof body.savedAt === 'number' ? body.savedAt : null;
    if (res.status === 409 || body.conflict) {
      const remote = (body.items ?? []).map((i) => validateLapCardLayout(i)).filter(Boolean) as LapCardLayout[];
      return { ok: false, conflict: true, items: remote, savedAt };
    }
    return { ok: res.ok && body.ok !== false, savedAt };
  } catch {
    return { ok: false }; // offline — the localStorage copy still holds the work
  }
}
