'use client';

import { useState, useEffect, useCallback, useRef } from 'react';
import { Button } from '@/components/ui/button';
import { AppState } from '@/lib/types';
import { useRouter } from 'next/navigation';

const EventInProgressPage = () => {
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

  const updateState = (updater: (prevState: AppState) => AppState) => {
    const newState = updater(appState);
    setAppState(newState);
    sendStateUpdate(newState);
  };

  const handleEndEvent = () => {
    updateState(s => ({ ...s, eventTracker: { isTimerRunning: false, timerStartTime: 0, timerBaseTime: 0, turns: [], accels: [], laps: [] }, newEvent: undefined, currentPage: '/event/new' }));
    router.push('/event/new');
  };

  const handleEndDriveDay = () => {
    updateState(s => ({ ...s, eventTracker: undefined, newEvent: undefined, driveDay: undefined, currentPage: '/driveday' }));
    router.push('/driveday');
  };

  return (
    <div className="container mx-auto p-8 flex flex-col items-center justify-center">
      <h1 className="text-4xl font-bold mb-8">Event in Progress</h1>
      <div className="flex space-x-4">
        <Button onClick={handleEndEvent} variant="destructive">End Event</Button>
        <Button onClick={handleEndDriveDay} variant="destructive">End Drive Day</Button>
      </div>
    </div>
  );
};

export default EventInProgressPage;