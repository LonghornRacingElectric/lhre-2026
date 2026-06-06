'use client';

// Client side of the trackside-live presence/leadership protocol (see
// app/api/motec/live/presence/route.ts). Heartbeats the server and reports this
// client's role. FAIL-OPEN: until the server explicitly says "mirror"/"full",
// the client is treated as full-control, so a presence glitch can never lock a
// solo strategist into read-only.

import { useEffect, useRef, useState } from 'react';

export type PresenceRole = 'connecting' | 'leader' | 'mirror' | 'full';

export interface Presence {
  role: PresenceRole;
  /** Read-only iff the server explicitly demoted us (mirror or cap-full). */
  isMirror: boolean;
  clients: number;
  max: number;
  leader: string | null;
  clientId: string;
}

const CLIENT_ID_KEY = 'trackside-client-id';
const HEARTBEAT_MS = 4000;

function getClientId(): string {
  let id = localStorage.getItem(CLIENT_ID_KEY);
  if (!id) {
    id = `c-${Math.random().toString(36).slice(2, 10)}-${Date.now().toString(36)}`;
    localStorage.setItem(CLIENT_ID_KEY, id);
  }
  return id;
}

export function usePresence(enabled: boolean): Presence {
  const [state, setState] = useState<Presence>({
    role: 'connecting', isMirror: false, clients: 1, max: 3, leader: null, clientId: '',
  });
  const stopRef = useRef(false);

  useEffect(() => {
    if (!enabled || typeof window === 'undefined') return;
    stopRef.current = false;
    const clientId = getClientId();

    const beat = async () => {
      try {
        const res = await fetch('/api/motec/live/presence', {
          method: 'POST',
          headers: { 'content-type': 'application/json' },
          body: JSON.stringify({ clientId }),
        });
        const j = await res.json();
        if (stopRef.current) return;
        const role: PresenceRole = j.role === 'leader' || j.role === 'mirror' || j.role === 'full' ? j.role : 'leader';
        setState({
          role,
          isMirror: role === 'mirror' || role === 'full',
          clients: typeof j.clients === 'number' ? j.clients : 1,
          max: typeof j.max === 'number' ? j.max : 3,
          leader: j.leader ?? null,
          clientId,
        });
      } catch {
        // Fail open: keep full control if the presence endpoint is unreachable.
        if (!stopRef.current) setState((s) => ({ ...s, role: s.role === 'connecting' ? 'leader' : s.role, isMirror: false, clientId }));
      }
    };

    void beat();
    const iv = window.setInterval(beat, HEARTBEAT_MS);
    const leave = () => {
      const payload = JSON.stringify({ clientId, leave: true });
      navigator.sendBeacon?.('/api/motec/live/presence', new Blob([payload], { type: 'application/json' }));
    };
    window.addEventListener('beforeunload', leave);

    return () => {
      stopRef.current = true;
      window.clearInterval(iv);
      window.removeEventListener('beforeunload', leave);
      leave();
    };
  }, [enabled]);

  return state;
}
