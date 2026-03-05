import { NextResponse } from 'next/server';
import { clearAllBuffers } from '@/lib/kafka/messageBuffer';

export async function POST() {
  try {
    clearAllBuffers();
    console.log('Cleared all Kafka message buffers');
    return NextResponse.json({ success: true });
  } catch (error) {
    console.error('Error clearing buffers:', error);
    return NextResponse.json({ error: 'Failed to clear buffers' }, { status: 500 });
  }
}
