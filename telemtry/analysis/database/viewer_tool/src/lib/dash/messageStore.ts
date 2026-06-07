// Server bridge for the shared driver-message library (/api/dash/messages).
// Same pattern as layoutStore.ts: localStorage is the offline cache + instant
// first render; the server copy is the shared source every client sees.

import { validateMessageLibrary, type MessageLibrary } from './dashMessages';

export async function fetchServerMessages(): Promise<MessageLibrary | null> {
  if (typeof fetch === 'undefined') return null;
  try {
    const res = await fetch('/api/dash/messages');
    if (!res.ok) return null;
    const body = await res.json();
    return validateMessageLibrary(body);
  } catch {
    return null; // offline / unreachable — caller keeps localStorage copy
  }
}

export async function saveServerMessages(lib: MessageLibrary): Promise<void> {
  if (typeof fetch === 'undefined') return;
  try {
    await fetch('/api/dash/messages', {
      method: 'PUT',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify(lib),
    });
  } catch {
    // offline — the localStorage copy still holds the work
  }
}
