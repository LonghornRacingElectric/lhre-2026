'use client';

import { useState, useEffect, useCallback, useRef } from 'react';
import { Button } from '@/components/ui/button';
import { AppState } from '@/lib/types';
import { useRouter } from 'next/navigation';
import Link from 'next/link';

const DrivedayActivePage = () => {
  const router = useRouter();
  const [clientId] = useState(() => crypto.randomUUID());
  const [appState, setAppState] = useState<AppState>({});
  const appStateRef = useRef(appState);
  appStateRef.current = appState;

  const sendStateUpdate = useCallback(async (newState: Partial<AppState>) => {
    const fullState = { ...appStateRef.current, ...newState, lastUpdatedBy: clientId };
    try {
      await fetch('/api/event-sync', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(fullState),
      });
    } catch (error) {
      console.error('Failed to send state update:', error);
    }
  }, [clientId]);

  useEffect(() => {
    const eventSource = new EventSource('/api/event-sync');
    eventSource.onmessage = (event) => {
      const newState: AppState = JSON.parse(event.data);
      setAppState(newState);
    };
    return () => eventSource.close();
  }, []);

  const handleEndDriveDay = async () => {
    await fetch('/api/end-event', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
    });
    const reset: AppState = {
      ...appStateRef.current,
      eventTracker: { isTimerRunning: false, timerStartTime: 0, timerBaseTime: 0, turns: [], accels: [], laps: [] },
      driveDay: undefined,
      currentPage: '/driveday',
      lastUpdatedBy: clientId,
    };
    setAppState(reset);
    sendStateUpdate(reset);
    router.push('/driveday');
  };

  return (
    <div className="container mx-auto p-8 flex flex-col items-center justify-center pt-20">
      <h1 className="text-4xl font-bold mb-8">Drive Day Active</h1>
      <div className="flex space-x-4">
        <Button onClick={handleEndDriveDay} variant="destructive">End Drive Day</Button>
        <Link href="/live-viewer">
          <Button variant="default">Go to Live Viewer</Button>
        </Link>
      </div>
    </div>
  );
};

export default DrivedayActivePage;
