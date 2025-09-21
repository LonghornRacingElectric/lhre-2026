
import { NextRequest, NextResponse } from 'next/server';
import { EventEmitter } from 'events';

const emitter = new EventEmitter();

let eventState = {}; // In-memory state

export async function GET() {
  const stream = new ReadableStream({
    start(controller) {
      const listener = (data: any) => {
        controller.enqueue(`data: ${JSON.stringify(data)}\n\n`);
      };
      emitter.on('update', listener);

      // Send initial state
      controller.enqueue(`data: ${JSON.stringify(eventState)}\n\n`);

      // Cleanup on client disconnect
      // The `controller.signal` is not standard, so we rely on a different mechanism
      // or just let it be for this example. In a real app, you'd need a robust way
      // to handle client disconnects.
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
    eventState = body;
    emitter.emit('update', eventState);
    return NextResponse.json({ success: true });
  } catch (error) {
    return NextResponse.json({ error: 'Invalid request' }, { status: 400 });
  }
}

