// Server bridge for the shared dash-layout library (/api/dash/layouts). Lets
// every client see the same saved layouts. localStorage stays as an offline
// cache + instant initial render; the server copy is the shared source.

import { validateLapCardLayout, type LapCardLayout } from './dashLayout';

export async function fetchServerLayouts(): Promise<LapCardLayout[] | null> {
  if (typeof fetch === 'undefined') return null;
  try {
    const res = await fetch('/api/dash/layouts');
    if (!res.ok) return null;
    const body = (await res.json()) as { items?: unknown[] };
    return (body.items ?? []).map((i) => validateLapCardLayout(i)).filter(Boolean) as LapCardLayout[];
  } catch {
    return null; // offline / unreachable — caller keeps localStorage copy
  }
}

export async function saveServerLayouts(items: LapCardLayout[]): Promise<void> {
  if (typeof fetch === 'undefined') return;
  try {
    await fetch('/api/dash/layouts', {
      method: 'PUT',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ items }),
    });
  } catch {
    // offline — the localStorage copy still holds the work
  }
}
