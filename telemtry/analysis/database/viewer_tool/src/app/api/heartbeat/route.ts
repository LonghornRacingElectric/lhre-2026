
import { NextRequest, NextResponse } from 'next/server';

const activeUsers = new Map<string, number>();
const TIMEOUT = 15000; // 15 seconds

export async function POST(request: NextRequest) {
  try {
    const { userId } = await request.json();
    if (userId) {
      activeUsers.set(userId, Date.now());
      return NextResponse.json({ success: true });
    }
    return NextResponse.json({ error: 'User ID is required' }, { status: 400 });
  } catch (error) {
    return NextResponse.json({ error: 'Invalid request' }, { status: 400 });
  }
}

export async function GET() {
  const now = Date.now();
  let activeCount = 0;
  for (const lastSeen of activeUsers.values()) {
    if (now - lastSeen <= TIMEOUT) {
      activeCount++;
    }
  }
  return NextResponse.json({ activeUsers: activeCount });
}
