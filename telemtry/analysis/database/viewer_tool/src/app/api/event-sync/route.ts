import { NextRequest, NextResponse } from 'next/server';
import { EventEmitter } from 'events';
import { AppState } from '@/lib/types';

const emitter = new EventEmitter();
emitter.setMaxListeners(100);

let appState: AppState = {}; // In-memory state

console.log("Initializing event-sync API route");

export async function GET() {
  console.log("Client connected to SSE");
  let listener: (data: AppState) => void;
  const stream = new ReadableStream({
    start(controller) {
      listener = (data: AppState) => {
        console.log("Broadcasting update to a client");
        controller.enqueue(`data: ${JSON.stringify(data)}\n\n`);
      };
      emitter.on('update', listener);
      console.log("Initial state sent to a client");
      controller.enqueue(`data: ${JSON.stringify(appState)}\n\n`);
    },
    cancel() {
      console.log("Client disconnected from SSE");
      emitter.off('update', listener);
    },
  });

  return new Response(stream, {
    headers: {
      'Content-Type': 'text/event-stream',
      'Cache-Control': 'no-cache',
      'Connection': 'keep-alive',
    },
  });
}

export async function POST(request: NextRequest) {
  try {
    const body = await request.json();
    appState = body;
    console.log("Received state update, broadcasting...");
    emitter.emit('update', appState);
    return NextResponse.json({ success: true });
  } catch (error) {
    console.error("Error processing POST request:", error);
    return NextResponse.json({ error: 'Invalid request' }, { status: 400 });
  }
}
