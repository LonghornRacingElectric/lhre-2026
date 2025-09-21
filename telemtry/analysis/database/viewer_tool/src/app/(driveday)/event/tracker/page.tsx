'use client';

import { useState, useEffect, useCallback, useRef } from 'react';
import dynamic from 'next/dynamic';
import { useStopwatch } from '@/hooks/useStopwatch';
import { Button } from '@/components/ui/button';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import {
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableHeader,
  TableRow,
} from '@/components/ui/table';
import { Input } from '@/components/ui/input';
import { AppState, EventTrackerState } from '@/lib/types';

const DynamicMap = dynamic(() => import('@/components/Map'), {
  ssr: false,
});

const EventTrackerPage = () => {
  const [activeUsers, setActiveUsers] = useState(0);
  const [appState, setAppState] = useState<AppState>({});
  const eventTrackerState = appState.eventTracker || {};

  const appStateRef = useRef(appState);
  appStateRef.current = appState;

  const { time, formattedTime, isRunning, start, stop, setTime, timeFormatter } = useStopwatch();
  const isRunningRef = useRef(isRunning);
  isRunningRef.current = isRunning;

  const sendStateUpdate = useCallback(async (newState: Partial<EventTrackerState>) => {
    const fullState: AppState = { ...appStateRef.current, eventTracker: { ...appStateRef.current.eventTracker, ...newState } };
    try {
      await fetch('/api/event-sync', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(fullState),
      });
    } catch (error) {
      console.error('Failed to send state update:', error);
    }
  }, []);

  useEffect(() => {
    const eventSource = new EventSource('/api/event-sync');
    eventSource.onmessage = (event) => {
      const newState: AppState = JSON.parse(event.data);
      setAppState(newState);

      const etState = newState.eventTracker || {};
      if (etState.isTimerRunning && etState.timerStartTime && etState.timerBaseTime !== undefined) {
        if (!isRunningRef.current) {
          start(etState.timerStartTime, etState.timerBaseTime);
        }
      } else if (!etState.isTimerRunning && etState.timerBaseTime !== undefined) {
        if (isRunningRef.current) {
          stop(etState.timerBaseTime);
        }
        setTime(etState.timerBaseTime);
      }
    };
    return () => eventSource.close();
  }, [start, stop, setTime]);

  // Heartbeat effect
  useEffect(() => {
    const userId = crypto.randomUUID();
    const heartbeat = async () => {
      try {
        await fetch('/api/heartbeat', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ userId }) });
        const response = await fetch('/api/heartbeat');
        const data = await response.json();
        setActiveUsers(data.activeUsers);
      } catch (error) {
        console.error('Heartbeat failed:', error);
      }
    };
    heartbeat();
    const interval = setInterval(heartbeat, 5000);
    return () => clearInterval(interval);
  }, []);

  const handleStart = () => sendStateUpdate({ isTimerRunning: true, timerStartTime: Date.now(), timerBaseTime: time });
  const handleStop = () => sendStateUpdate({ isTimerRunning: false, timerBaseTime: time });
  const handleReset = () => sendStateUpdate({ isTimerRunning: false, timerStartTime: 0, timerBaseTime: 0, turns: [], accels: [], laps: [] });

  const handleToggleTurn = () => {
    const { isTurning, turnStartTime, turns = [] } = eventTrackerState;
    if (isTurning) {
      sendStateUpdate({ isTurning: false, turns: [...turns, { startTime: turnStartTime || 0, endTime: time }] });
    } else {
      sendStateUpdate({ isTurning: true, turnStartTime: time });
    }
  };

  const handleToggleAccel = () => {
    const { isAccel, accelStartTime, accels = [] } = eventTrackerState;
    if (isAccel) {
      sendStateUpdate({ isAccel: false, accels: [...accels, { startTime: accelStartTime || 0, endTime: time }] });
    } else {
      sendStateUpdate({ isAccel: true, accelStartTime: time });
    }
  };

  const handleAddLap = () => sendStateUpdate({ laps: [...(eventTrackerState.laps || []), time] });

  const handleNoteChange = (table: 'turns' | 'accels', index: number, notes: string) => {
    const tableData = eventTrackerState[table] || [];
    const newTableData = [...tableData];
    newTableData[index] = { ...newTableData[index], notes };
    sendStateUpdate({ [table]: newTableData });
  };

  return (
    <div className="container mx-auto p-8">
      <div className="fixed top-16 left-4 bg-white p-2 rounded-lg shadow-md">Active Users: {activeUsers}</div>
      <div className="grid grid-cols-1 md:grid-cols-2 gap-8">
        <Card>
          <CardHeader><CardTitle className="text-center">Timer</CardTitle></CardHeader>
          <CardContent className="flex flex-col items-center">
            <div id="timer" className="text-6xl font-mono text-center mb-4">{formattedTime}</div>
            <div className="flex justify-center space-x-4 mb-4">
              <Button onClick={handleStart}>START</Button>
              <Button onClick={handleStop} variant="destructive">STOP</Button>
              <Button onClick={handleReset} variant="secondary">End Event</Button>
            </div>
            <div className="flex justify-center space-x-4">
              <Button onClick={handleToggleTurn} variant="outline">{eventTrackerState.isTurning ? 'End Turn' : 'Start Turn'}</Button>
              <Button onClick={handleToggleAccel} variant="outline">{eventTrackerState.isAccel ? 'End Accel' : 'Start Accel'}</Button>
              <Button onClick={handleAddLap} variant="outline">Add Lap</Button>
            </div>
          </CardContent>
        </Card>
        <div className="space-y-8">
          <Card><CardHeader><CardTitle>Map</CardTitle></CardHeader><CardContent className="h-64"><DynamicMap /></CardContent></Card>
          <Card><CardHeader><CardTitle>Live Data</CardTitle></CardHeader><CardContent className="h-64 bg-gray-200 rounded-md"></CardContent></Card>
        </div>
      </div>
      <div className="mt-8 grid grid-cols-1 lg:grid-cols-3 gap-8">
        <Card>
          <CardHeader><CardTitle>Turn Table</CardTitle></CardHeader>
          <CardContent>
            <Table>
              <TableHeader><TableRow><TableHead>Turn #</TableHead><TableHead>Start</TableHead><TableHead>End</TableHead><TableHead>Notes</TableHead></TableRow></TableHeader>
              <TableBody>{(eventTrackerState.turns || []).map((t, i) => <TableRow key={i}><TableCell>{i + 1}</TableCell><TableCell>{timeFormatter(t.startTime)}</TableCell><TableCell>{timeFormatter(t.endTime)}</TableCell><TableCell><Input value={t.notes || ''} onChange={(e) => handleNoteChange('turns', i, e.target.value)} /></TableCell></TableRow>)}</TableBody>
            </Table>
          </CardContent>
        </Card>
        <Card>
          <CardHeader><CardTitle>Acceleration Table</CardTitle></CardHeader>
          <CardContent>
            <Table>
              <TableHeader><TableRow><TableHead>Accel #</TableHead><TableHead>Start</TableHead><TableHead>End</TableHead><TableHead>Notes</TableHead></TableRow></TableHeader>
              <TableBody>{(eventTrackerState.accels || []).map((a, i) => <TableRow key={i}><TableCell>{i + 1}</TableCell><TableCell>{timeFormatter(a.startTime)}</TableCell><TableCell>{timeFormatter(a.endTime)}</TableCell><TableCell><Input value={a.notes || ''} onChange={(e) => handleNoteChange('accels', i, e.target.value)} /></TableCell></TableRow>)}</TableBody>
            </Table>
          </CardContent>
        </Card>
        <Card>
          <CardHeader><CardTitle>Lap Table</CardTitle></CardHeader>
          <CardContent>
            <Table>
              <TableHeader><TableRow><TableHead>Lap #</TableHead><TableHead>Time</TableHead></TableRow></TableHeader>
              <TableBody>{(eventTrackerState.laps || []).map((l, i) => <TableRow key={i}><TableCell>{i + 1}</TableCell><TableCell>{timeFormatter(l)}</TableCell></TableRow>)}</TableBody>
            </Table>
          </CardContent>
        </Card>
      </div>
    </div>
  );
};

export default EventTrackerPage;