// Lightweight presence/leadership for multi-client trackside-live.
//
// Every browser runs lap detection on the same stream, so if they all wrote the
// shared session we'd get duplicate laps. Instead one client is the LEADER
// (owns the session: logs laps, sets targets, writes the session-cache) and the
// rest are MIRRORS (read the shared doc). The first still-alive client by claim
// time is the leader; if it leaves, the next is promoted. Capped at MAX_CLIENTS.
//
// State is in-process (single `next start` instance) — fine for a trackside box.
// Clients heartbeat ~every 4s; a client is dropped after TTL_MS of silence.
export const runtime = "nodejs";
export const dynamic = "force-dynamic";

import { NextRequest, NextResponse } from "next/server";

type Client = { id: string; firstSeen: number; lastSeen: number };

const TTL_MS = 12_000;
const MAX_CLIENTS = 3;

const g = globalThis as unknown as { __livePresence?: Map<string, Client> };
const clients = g.__livePresence ?? (g.__livePresence = new Map<string, Client>());

function prune(now: number) {
  for (const [id, c] of clients) if (now - c.lastSeen > TTL_MS) clients.delete(id);
}

function leaderId(): string | null {
  let leader: Client | null = null;
  for (const c of clients.values()) {
    if (!leader || c.firstSeen < leader.firstSeen || (c.firstSeen === leader.firstSeen && c.id < leader.id)) {
      leader = c;
    }
  }
  return leader?.id ?? null;
}

export async function POST(req: NextRequest) {
  const now = Date.now();
  const body = (await req.json().catch(() => ({}))) as { clientId?: string; leave?: boolean };
  const clientId = typeof body.clientId === "string" ? body.clientId : "";
  if (!clientId) return NextResponse.json({ error: "clientId required" }, { status: 400 });

  prune(now);

  if (body.leave) {
    clients.delete(clientId);
    return NextResponse.json({ role: "left", clients: clients.size, max: MAX_CLIENTS, leader: leaderId() });
  }

  const existing = clients.get(clientId);
  if (existing) {
    existing.lastSeen = now;
  } else if (clients.size < MAX_CLIENTS) {
    clients.set(clientId, { id: clientId, firstSeen: now, lastSeen: now });
  } else {
    // Cap reached — this client gets no slot (read-only, no leadership).
    return NextResponse.json({ role: "full", clients: clients.size, max: MAX_CLIENTS, leader: leaderId() });
  }

  const leader = leaderId();
  return NextResponse.json({
    role: leader === clientId ? "leader" : "mirror",
    leader,
    clients: clients.size,
    max: MAX_CLIENTS,
  });
}
