'use client';

// Single source of truth for the driver-message library, shared by the Dash-tab
// quick-send row and the message editor (both call this hook from the SAME
// TracksideApp instance, so they see one state). localStorage = offline cache +
// instant first paint; the server copy is the shared, all-clients source.

import { useCallback, useEffect, useRef, useState } from 'react';
import {
  defaultMessageLibrary, validateMessageLibrary, newMessage, MAX_ACTIVE_MESSAGES,
  type MessageLibrary, type DashMessage,
} from './dashMessages';
import { fetchServerMessages, saveServerMessages } from './messageStore';

const LIBRARY_KEY = 'dash-driver-messages';

function loadLibrary(): MessageLibrary {
  if (typeof window !== 'undefined') {
    try {
      const raw = localStorage.getItem(LIBRARY_KEY);
      if (raw) {
        const v = validateMessageLibrary(JSON.parse(raw));
        if (v && v.items.length) return v;
      }
    } catch { /* fall through */ }
  }
  return defaultMessageLibrary();
}

export interface MessageLibraryApi {
  lib: MessageLibrary;
  syncMsg: string;
  pullFromServer: (announce?: boolean) => Promise<void>;
  addMessage: () => string;            // returns new id
  duplicateMessage: (id: string) => void;
  updateMessage: (id: string, patch: Partial<DashMessage>) => void;
  removeMessage: (id: string) => void;
  toggleActive: (id: string) => void;  // add/remove from active set (capped)
  isActive: (id: string) => boolean;
  atActiveCap: boolean;
}

export function useMessageLibrary(): MessageLibraryApi {
  const [lib, setLib] = useState<MessageLibrary>(loadLibrary);
  const [syncMsg, setSyncMsg] = useState('Syncing…');
  const libRef = useRef(lib);
  libRef.current = lib;

  // Persist locally on every change; debounce a push to the shared server copy.
  const serverSaveTimer = useRef<number | null>(null);
  useEffect(() => {
    try { localStorage.setItem(LIBRARY_KEY, JSON.stringify(lib)); } catch { /* quota */ }
    if (serverSaveTimer.current) window.clearTimeout(serverSaveTimer.current);
    serverSaveTimer.current = window.setTimeout(() => { void saveServerMessages(lib); }, 800);
    return () => { if (serverSaveTimer.current) window.clearTimeout(serverSaveTimer.current); };
  }, [lib]);

  const pullFromServer = useCallback(async (announce = true) => {
    if (announce) setSyncMsg('Syncing…');
    const server = await fetchServerMessages();
    if (server === null) { setSyncMsg('Offline — local only'); return; }
    if (server.items.length) { setLib(server); setSyncMsg('Synced'); }
    else { setSyncMsg('Synced'); void saveServerMessages(libRef.current); } // seed empty server
  }, []);
  // Pull the shared library once on mount.
  useEffect(() => { void pullFromServer(); /* eslint-disable-next-line react-hooks/exhaustive-deps */ }, []);

  const addMessage = useCallback((): string => {
    const m = newMessage();
    setLib((p) => ({ ...p, items: [...p.items, m] }));
    return m.id;
  }, []);

  const duplicateMessage = useCallback((id: string) => {
    setLib((p) => {
      const src = p.items.find((m) => m.id === id);
      if (!src) return p;
      const copy = { ...src, id: newMessage().id, label: `${src.label} copy` };
      return { ...p, items: [...p.items, copy] };
    });
  }, []);

  const updateMessage = useCallback((id: string, patch: Partial<DashMessage>) => {
    setLib((p) => ({ ...p, items: p.items.map((m) => (m.id === id ? { ...m, ...patch } : m)) }));
  }, []);

  const removeMessage = useCallback((id: string) => {
    setLib((p) => ({
      items: p.items.filter((m) => m.id !== id),
      activeIds: p.activeIds.filter((a) => a !== id),
    }));
  }, []);

  const toggleActive = useCallback((id: string) => {
    setLib((p) => {
      if (p.activeIds.includes(id)) return { ...p, activeIds: p.activeIds.filter((a) => a !== id) };
      if (p.activeIds.length >= MAX_ACTIVE_MESSAGES) return p; // capped — ignore
      return { ...p, activeIds: [...p.activeIds, id] };
    });
  }, []);

  const isActive = useCallback((id: string) => lib.activeIds.includes(id), [lib.activeIds]);

  return {
    lib, syncMsg, pullFromServer,
    addMessage, duplicateMessage, updateMessage, removeMessage, toggleActive, isActive,
    atActiveCap: lib.activeIds.length >= MAX_ACTIVE_MESSAGES,
  };
}
