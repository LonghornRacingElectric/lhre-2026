'use client';

import { useState, useEffect, useCallback } from 'react';
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

const DynamicMap = dynamic(() => import('@/components/Map'), {
  ssr: false,
});

interface LapData {
  startTime: number;
  endTime: number;
  notes?: string;
}

interface EventState {
  isTimerRunning?: boolean;
  timerStartTime?: number;
  timerInternalTime?: number;
  turns?: LapData[];
  accels?: LapData[];
  laps?: number[];
}

const EventTrackerPage = () => {
  const {
    time, formattedTime, isRunning, start, stop, reset,
    isTurning, toggleTurn, turns, setTurns,
    isAccel, toggleAccel, accels, setAccels,
    laps, setLaps, addLap,
    timeFormatter
  } = useStopwatch();

  const [activeUsers, setActiveUsers] = useState(0);
  const [eventState, setEventState] = useState<EventState>({});

  const sendStateUpdate = useCallback(async (newState: Partial<EventState>) => {
    const fullState = { ...eventState, ...newState };
    try {
      await fetch('/api/event-sync', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(fullState),
      });
    } catch (error) {
      console.error('Failed to send state update:', error);
    }
  }, [eventState]);

  useEffect(() => {
    const eventSource = new EventSource('/api/event-sync');
    eventSource.onmessage = (event) => {
      const newState: EventState = JSON.parse(event.data);
      setEventState(newState);

      if (newState.isTimerRunning && !isRunning) {
        start();
      } else if (!newState.isTimerRunning && isRunning) {
        stop();
      }

      if (newState.turns) setTurns(newState.turns);
      if (newState.accels) setAccels(newState.accels);
      if (newState.laps) setLaps(newState.laps);
    };
    return () => eventSource.close();
  }, [isRunning, start, stop, setTurns, setAccels, setLaps]);

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

  const handleStart = () => { start(); sendStateUpdate({ isTimerRunning: true, timerStartTime: Date.now(), timerInternalTime: time }); };
  const handleStop = () => { stop(); sendStateUpdate({ isTimerRunning: false, timerInternalTime: time }); };
  const handleReset = () => { reset(); sendStateUpdate({ isTimerRunning: false, timerInternalTime: 0, turns: [], accels: [], laps: [] }); };
  const handleToggleTurn = () => { const newTurns = toggleTurn(); sendStateUpdate({ turns: newTurns }); };
  const handleToggleAccel = () => { const newAccels = toggleAccel(); sendStateUpdate({ accels: newAccels }); };
  const handleAddLap = () => { const newLaps = addLap(); sendStateUpdate({ laps: newLaps }); };

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
              <Button onClick={handleToggleTurn} variant="outline">{isTurning ? 'End Turn' : 'Start Turn'}</Button>
              <Button onClick={handleToggleAccel} variant="outline">{isAccel ? 'End Accel' : 'Start Accel'}</Button>
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
              <TableBody>{turns.map((t, i) => <TableRow key={i}><TableCell>{i + 1}</TableCell><TableCell>{timeFormatter(t.startTime)}</TableCell><TableCell>{timeFormatter(t.endTime)}</TableCell><TableCell></TableCell></TableRow>)}</TableBody>
            </Table>
          </CardContent>
        </Card>
        <Card>
          <CardHeader><CardTitle>Acceleration Table</CardTitle></CardHeader>
          <CardContent>
            <Table>
              <TableHeader><TableRow><TableHead>Accel #</TableHead><TableHead>Start</TableHead><TableHead>End</TableHead><TableHead>Notes</TableHead></TableRow></TableHeader>
              <TableBody>{accels.map((a, i) => <TableRow key={i}><TableCell>{i + 1}</TableCell><TableCell>{timeFormatter(a.startTime)}</TableCell><TableCell>{timeFormatter(a.endTime)}</TableCell><TableCell></TableCell></TableRow>)}</TableBody>
            </Table>
          </CardContent>
        </Card>
        <Card>
          <CardHeader><CardTitle>Lap Table</CardTitle></CardHeader>
          <CardContent>
            <Table>
              <TableHeader><TableRow><TableHead>Lap #</TableHead><TableHead>Time</TableHead></TableRow></TableHeader>
              <TableBody>{laps.map((l, i) => <TableRow key={i}><TableCell>{i + 1}</TableCell><TableCell>{timeFormatter(l)}</TableCell></TableRow>)}</TableBody>
            </Table>
          </CardContent>
        </Card>
      </div>
    </div>
  );
};

export default EventTrackerPage;