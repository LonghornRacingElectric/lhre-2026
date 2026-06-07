'use client';

// Cross-client sync for the race plan (total laps + usable energy budget) via
// /api/dash/raceplan. Every client polls; whoever edits pushes. Last-writer-wins
// with a short local-edit suppression window so a poll can't clobber what you're
// typing or echo your own change back. `onRemote` is called when another client's
// value should be adopted.

import { useCallback, useEffect, useRef } from 'react';

export interface RacePlan { totalLaps: number; budgetKwh: number; soeCutoffCellV?: number; savedAt: number; }

const POLL_MS = 4000;
const LOCAL_EDIT_GRACE_MS = 6000; // ignore server for this long after a local edit

export function useRacePlan(onRemote: (p: RacePlan) => void): { push: (totalLaps: number, budgetKwh: number, soeCutoffCellV?: number) => void } {
  const lastLocalEditRef = useRef(0);
  const onRemoteRef = useRef(onRemote);
  onRemoteRef.current = onRemote;

  const push = useCallback((totalLaps: number, budgetKwh: number, soeCutoffCellV?: number) => {
    lastLocalEditRef.current = Date.now();
    void fetch('/api/dash/raceplan', {
      method: 'PUT',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ totalLaps, budgetKwh, soeCutoffCellV }),
    }).catch(() => { /* offline — local value still holds */ });
  }, []);

  useEffect(() => {
    let alive = true;
    const poll = async () => {
      try {
        const r = await fetch('/api/dash/raceplan');
        if (!r.ok) return;
        const p = (await r.json()) as RacePlan;
        if (!alive || !p || typeof p.totalLaps !== 'number') return;
        // Don't adopt while the user just edited (let our push propagate first).
        if (Date.now() - lastLocalEditRef.current < LOCAL_EDIT_GRACE_MS) return;
        onRemoteRef.current(p);
      } catch { /* offline */ }
    };
    void poll();
    const id = window.setInterval(poll, POLL_MS);
    return () => { alive = false; window.clearInterval(id); };
  }, []);

  return { push };
}
