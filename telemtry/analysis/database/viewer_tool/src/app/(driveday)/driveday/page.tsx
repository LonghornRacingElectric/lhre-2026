'use client';

import { useState, useEffect, useCallback, useRef } from 'react';
import { useRouter } from 'next/navigation';
import { Button } from "@/components/ui/button";
import {
  Card,
  CardContent,
  CardDescription,
  CardFooter,
  CardHeader,
  CardTitle,
} from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { AppState, DriveDayState } from '@/lib/types';

export default function DrivedayPage() {
  const router = useRouter();
  const [clientId] = useState(() => crypto.randomUUID());
  const [appState, setAppState] = useState<AppState>({});
  const [driveDayState, setDriveDayState] = useState<DriveDayState>({});

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
      if (newState.lastUpdatedBy !== clientId && newState.driveDay) {
        setDriveDayState(newState.driveDay);
      }
    };
    return () => eventSource.close();
  }, [clientId]);

  const handleInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const { id, value } = e.target;
    const newDriveDayState = { ...driveDayState, [id]: value };
    setDriveDayState(newDriveDayState);
    sendStateUpdate({ driveDay: newDriveDayState });
  };

  const handleCreate = async () => {
    const response = await fetch('/api/new-drive-day', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        power_limit: driveDayState.powerLimit,
        conditions: driveDayState.drivingConditions,
      }),
    });
    if(response.status != 201) {
      console.error('Failed to create new drive day');
      return;
    }
    sendStateUpdate({ currentPage: '/event/new' });
    router.push('/event/new');
  };

  return (
    <div className="p-8 flex justify-center items-center">
      <Card className="w-[350px]">
        <CardHeader>
          <CardTitle>Create New Drive Day</CardTitle>
          <CardDescription>Enter the details for the new drive day.</CardDescription>
        </CardHeader>
        <CardContent>
          <form>
            <div className="grid w-full items-center gap-4">
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="powerLimit">Power Limit</Label>
                <Input id="powerLimit" placeholder="Enter power limit" value={driveDayState.powerLimit || ''} onChange={handleInputChange} />
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="drivingConditions">Driving Conditions</Label>
                <Input id="drivingConditions" placeholder="Enter driving conditions" value={driveDayState.drivingConditions || ''} onChange={handleInputChange} />
              </div>
            </div>
          </form>
        </CardContent>
        <CardFooter className="flex justify-between">
          <Button variant="outline">Cancel</Button>
          <Button onClick={handleCreate}>Create</Button>
        </CardFooter>
      </Card>
    </div>
  );
}