import { NextRequest, NextResponse } from 'next/server';
import { EventEmitter } from 'events';
import { AppState } from '@/lib/types';

const emitter = new EventEmitter();
emitter.setMaxListeners(100);

let appState: AppState = {}; // In-memory state

export async function GET() {
  let listener: (data: AppState) => void;
  const stream = new ReadableStream({
    start(controller) {
      listener = (data: AppState) => {
        controller.enqueue(`data: ${JSON.stringify(data)}\n\n`);
      };
      emitter.on('update', listener);
      controller.enqueue(`data: ${JSON.stringify(appState)}\n\n`);
    },
    cancel() {
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
    appState = { ...appState, ...body }; // Merge the new state
    emitter.emit('update', appState);
    return NextResponse.json({ success: true });
  } catch (error) {
    return NextResponse.json({ error: 'Invalid request' }, { status: 400 });
  }
}
