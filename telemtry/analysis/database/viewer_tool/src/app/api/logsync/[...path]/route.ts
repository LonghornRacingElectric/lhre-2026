// Thin reverse-proxy from the viewer to the logsync worker service.
//
// Keeps the browser same-origin (no CORS) and gives us one place to add auth
// later. Streams responses through untouched so SSE (`/events`) and large file
// downloads (`/jobs/{id}/files/{name}`, `/archive`) work without buffering.
export const runtime = "nodejs";
export const dynamic = "force-dynamic";

import { NextRequest } from "next/server";

const LOGSYNC_URL = (process.env.LOGSYNC_URL ?? "http://localhost:8090").replace(/\/$/, "");

// Upstream headers worth surfacing to the browser.
const PASS_THROUGH = [
  "content-type",
  "content-disposition",
  "content-length",
  "cache-control",
  "x-accel-buffering",
];

async function proxy(req: NextRequest, path: string[]) {
  const search = req.nextUrl.search; // includes leading '?'
  const target = `${LOGSYNC_URL}/${path.join("/")}${search}`;

  const init: RequestInit & { duplex?: "half" } = {
    method: req.method,
    headers: {},
    // upstream SSE/downloads can take a long while
    signal: req.signal,
  };

  if (req.method !== "GET" && req.method !== "HEAD") {
    const body = await req.text();
    if (body) {
      init.body = body;
      const ct = req.headers.get("content-type");
      (init.headers as Record<string, string>)["content-type"] =
        ct ?? "application/json";
    }
  }

  let upstream: Response;
  try {
    upstream = await fetch(target, init);
  } catch (e) {
    return new Response(
      JSON.stringify({ error: `logsync worker unreachable: ${String(e)}` }),
      { status: 502, headers: { "content-type": "application/json" } },
    );
  }

  const headers = new Headers();
  for (const h of PASS_THROUGH) {
    const v = upstream.headers.get(h);
    if (v) headers.set(h, v);
  }

  return new Response(upstream.body, { status: upstream.status, headers });
}

type Ctx = { params: Promise<{ path: string[] }> };

export async function GET(req: NextRequest, ctx: Ctx) {
  return proxy(req, (await ctx.params).path);
}
export async function POST(req: NextRequest, ctx: Ctx) {
  return proxy(req, (await ctx.params).path);
}
export async function DELETE(req: NextRequest, ctx: Ctx) {
  return proxy(req, (await ctx.params).path);
}
