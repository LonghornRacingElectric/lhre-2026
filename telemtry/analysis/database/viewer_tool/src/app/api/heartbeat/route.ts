import { NextRequest, NextResponse } from 'next/server';
import prisma from '@/lib/prisma';

const TIMEOUT = 15000; // 15 seconds

export async function POST(request: NextRequest) {
  try {
    const { userId } = await request.json();
    if (userId) {
      await prisma.activeUser.upsert({
        where: { userId },
        update: { lastSeen: new Date() },
        create: { userId, lastSeen: new Date() },
      });
      return NextResponse.json({ success: true });
    }
    return NextResponse.json({ error: 'User ID is required' }, { status: 400 });
  } catch (error) {
    return NextResponse.json({ error: 'Invalid request' }, { status: 400 });
  }
}

export async function GET() {
  const timeout = new Date(Date.now() - TIMEOUT);
  const activeCount = await prisma.activeUser.count({
    where: {
      lastSeen: {
        gte: timeout,
      },
    },
  });
  return NextResponse.json({ activeUsers: activeCount });
}