'use client';

// Client side of the trackside-live presence/leadership protocol (see
// app/api/motec/live/presence/route.ts). Heartbeats the server and reports this
// client's role. FAIL-OPEN: until the server explicitly says "mirror"/"full",
// the client is treated as full-control, so a presence glitch can never lock a
// solo strategist into read-only. Also supports manual leadership transfer:
// a mirror can requestLeader(); the leader sees `requests` and grantLeader(id).

import { useCallback, useEffect, useRef, useState } from 'react';

export type PresenceRole = 'connecting' | 'leader' | 'mirror' | 'full';

export interface PresenceRequest { id: string; name: string }

export interface Presence {
  role: PresenceRole;
  /** Read-only iff the server explicitly demoted us (mirror or cap-full). */
  isMirror: boolean;
  clients: number;
  max: number;
  leader: string | null;
  clientId: string;
  /** This client's display name (editable). */
  name: string;
  setName: (n: string) => void;
  /** Pending control requests — for the leader's transfer popup. */
  requests: PresenceRequest[];
  /** Whether THIS client has an outstanding control request. */
  requested: boolean;
  requestLeader: () => void;
  grantLeader: (target: string) => void;
  denyLeader: (target: string) => void;
  /** Admin only: instantly seize leadership (server verifies isAdmin). */
  forceLeader: () => void;
  /** Session starter reclaim: instantly seize leadership on rejoin. Gated
   *  client-side by sessionInfo.starterId === clientId. */
  claimAsStarter: () => void;
}

const CLIENT_ID_KEY = 'trackside-client-id';
const CLIENT_NAME_KEY = 'trackside-client-name';
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
  const [s, setS] = useState<Omit<Presence, 'requestLeader' | 'grantLeader' | 'denyLeader' | 'setName' | 'forceLeader' | 'claimAsStarter'>>({
    role: 'connecting', isMirror: false, clients: 1, max: 3, leader: null, clientId: '', name: '', requests: [], requested: false,
  });
  const stopRef = useRef(false);
  const idRef = useRef('');
  const nameRef = useRef('');

  const apply = useCallback((j: Record<string, unknown>, clientId: string) => {
    const role: PresenceRole = j.role === 'leader' || j.role === 'mirror' || j.role === 'full' ? j.role : 'leader';
    setS({
      role,
      isMirror: role === 'mirror' || role === 'full',
      clients: typeof j.clients === 'number' ? j.clients : 1,
      max: typeof j.max === 'number' ? j.max : 3,
      leader: (j.leader as string) ?? null,
      clientId,
      name: nameRef.current,
      requests: Array.isArray(j.requests) ? (j.requests as PresenceRequest[]) : [],
      requested: !!j.requested,
    });
  }, []);

  const post = useCallback(async (extra: Record<string, unknown>) => {
    const clientId = idRef.current; if (!clientId) return;
    try {
      const res = await fetch('/api/motec/live/presence', {
        method: 'POST', headers: { 'content-type': 'application/json' },
        body: JSON.stringify({ clientId, name: nameRef.current, ...extra }),
      });
      const j = await res.json();
      if (!stopRef.current) apply(j, clientId);
    } catch {
      if (!stopRef.current) setS((p) => ({ ...p, role: p.role === 'connecting' ? 'leader' : p.role, isMirror: false }));
    }
  }, [apply]);

  const setName = useCallback((n: string) => {
    nameRef.current = n;
    try { localStorage.setItem(CLIENT_NAME_KEY, n); } catch { /* quota */ }
    setS((p) => ({ ...p, name: n }));
    void post({});
  }, [post]);

  useEffect(() => {
    if (!enabled || typeof window === 'undefined') return;
    stopRef.current = false;
    idRef.current = getClientId();
    nameRef.current = localStorage.getItem(CLIENT_NAME_KEY) || '';
    setS((p) => ({ ...p, name: nameRef.current }));
    void post({});
    const iv = window.setInterval(() => post({}), HEARTBEAT_MS);
    const leave = () => {
      const payload = JSON.stringify({ clientId: idRef.current, leave: true });
      navigator.sendBeacon?.('/api/motec/live/presence', new Blob([payload], { type: 'application/json' }));
    };
    window.addEventListener('beforeunload', leave);
    return () => { stopRef.current = true; window.clearInterval(iv); window.removeEventListener('beforeunload', leave); leave(); };
  }, [enabled, post]);

  const requestLeader = useCallback(() => { void post({ action: 'request' }); }, [post]);
  const grantLeader = useCallback((target: string) => { void post({ action: 'grant', target }); }, [post]);
  const denyLeader = useCallback((target: string) => { void post({ action: 'deny', target }); }, [post]);
  const forceLeader = useCallback(() => { void post({ action: 'force' }); }, [post]);
  const claimAsStarter = useCallback(() => { void post({ action: 'claim_starter' }); }, [post]);

  return { ...s, setName, requestLeader, grantLeader, denyLeader, forceLeader, claimAsStarter };
}
