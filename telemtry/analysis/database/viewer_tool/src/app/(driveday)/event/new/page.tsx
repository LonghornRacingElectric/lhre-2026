
'use client';

import { useState, useEffect, useCallback, useRef } from 'react';
import { useRouter } from 'next/navigation';
import { Button } from "@/components/ui/button";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/ui/card";
import { Checkbox } from "@/components/ui/checkbox";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import { AppState, NewEventState } from '@/lib/types';

export default function NewEventPage() {
  const router = useRouter();
  const [clientId] = useState(() => crypto.randomUUID());
  const [appState, setAppState] = useState<AppState>({});
  const [newEventState, setNewEventState] = useState<NewEventState>({});

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
      if (newState.lastUpdatedBy !== clientId && newState.newEvent) {
        setNewEventState(newState.newEvent);
      }
    };
    return () => eventSource.close();
  }, [clientId]);

  const handleInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const { id, value } = e.target;
    const newFormState = { ...newEventState, [id]: value };
    setNewEventState(newFormState);
    sendStateUpdate({ newEvent: newFormState });
  };

  const handleCheckboxChange = (id: keyof NewEventState, checked: boolean) => {
    const newFormState = { ...newEventState, [id]: checked };
    setNewEventState(newFormState);
    sendStateUpdate({ newEvent: newFormState });
  };

  const handleSelectChange = (id: keyof NewEventState, value: string) => {
    const newFormState = { ...newEventState, [id]: value };
    setNewEventState(newFormState);
    sendStateUpdate({ newEvent: newFormState });
  };

  const handleCreateEvent = () => {
    sendStateUpdate({ currentPage: '/event/tracker' });
    router.push('/event/tracker');
  };

  return (
    <div className="p-8 flex justify-center items-center">
      <Card className="w-full max-w-4xl">
        <CardHeader>
          <CardTitle>Create New Event</CardTitle>
          <CardDescription>Enter the details for the new event.</CardDescription>
        </CardHeader>
        <CardContent>
          <form>
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="driverId">Choose a driver</Label>
                <Select onValueChange={(v) => handleSelectChange('driverId', v)} value={newEventState.driverId}><SelectTrigger><SelectValue placeholder="Select a driver" /></SelectTrigger><SelectContent>
                  <SelectItem value="0">Other</SelectItem>
                  <SelectItem value="1">Rylan Hanks</SelectItem>
                  <SelectItem value="2">Sohan Agnihotri</SelectItem>
                  <SelectItem value="3">Dylan Hammerback</SelectItem>
                  <SelectItem value="4">Andrew Cloran</SelectItem>
                  <SelectItem value="5">Ali Jensen</SelectItem>
                  <SelectItem value="6">David Easter</SelectItem>
                </SelectContent></Select>
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="locationId">Choose a location</Label>
                <Select onValueChange={(v) => handleSelectChange('locationId', v)} value={newEventState.locationId}><SelectTrigger><SelectValue placeholder="Select a location" /></SelectTrigger><SelectContent>
                  <SelectItem value="0">Other</SelectItem>
                  <SelectItem value="1">Pickle - Innovation Blvd</SelectItem>
                  <SelectItem value="2">Pickle - Front Lot</SelectItem>
                  <SelectItem value="3">Pickle - Other Lot</SelectItem>
                  <SelectItem value="4">COTA - Lot J</SelectItem>
                  <SelectItem value="5">COTA - Lot H</SelectItem>
                  <SelectItem value="6">COTA - Go Kart Track</SelectItem>
                </SelectContent></Select>
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="eventType">Choose an event type</Label>
                <Select onValueChange={(v) => handleSelectChange('eventType', v)} value={newEventState.eventType}><SelectTrigger><SelectValue placeholder="Select an event type" /></SelectTrigger><SelectContent>
                  <SelectItem value="0">Other</SelectItem>
                  <SelectItem value="1">Endurance</SelectItem>
                  <SelectItem value="2">Autocross</SelectItem>
                  <SelectItem value="3">Skidpad</SelectItem>
                  <SelectItem value="4">Straight Line Acceleration</SelectItem>
                  <SelectItem value="5">Straight Line Breaking</SelectItem>
                </SelectContent></Select>
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="carId">Choose a car</Label>
                <Select onValueChange={(v) => handleSelectChange('carId', v)} value={newEventState.carId}><SelectTrigger><SelectValue placeholder="Select a car" /></SelectTrigger><SelectContent>
                  <SelectItem value="1">Easy Driver</SelectItem>
                  <SelectItem value="2">Lady Luck</SelectItem>
                </SelectContent></Select>
              </div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="carWeight">Car Weight</Label><Input id="carWeight" type="number" value={newEventState.carWeight || ''} onChange={handleInputChange} /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="towAngle">Tow Angle</Label><Input id="towAngle" type="number" value={newEventState.towAngle || ''} onChange={handleInputChange} /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="camber">Camber</Label><Input id="camber" type="number" value={newEventState.camber || ''} onChange={handleInputChange} /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="rideHeight">Ride Height</Label><Input id="rideHeight" type="number" value={newEventState.rideHeight || ''} onChange={handleInputChange} /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="ackermanAdjustment">Ackerman Adjustment</Label><Input id="ackermanAdjustment" type="number" value={newEventState.ackermanAdjustment || ''} onChange={handleInputChange} /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="powerLimit">Power Limit</Label><Input id="powerLimit" type="number" value={newEventState.powerLimit || ''} onChange={handleInputChange} /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="shockDampening">Shock Dampening</Label><Input id="shockDampening" type="number" value={newEventState.shockDampening || ''} onChange={handleInputChange} /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="torqueLimit">Torque Limit</Label><Input id="torqueLimit" type="number" value={newEventState.torqueLimit || ''} onChange={handleInputChange} /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="frwPressure">Front Right Wheel Pressure</Label><Input id="frwPressure" type="number" value={newEventState.frwPressure || ''} onChange={handleInputChange} /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="flwPressure">Front Left Wheel Pressure</Label><Input id="flwPressure" type="number" value={newEventState.flwPressure || ''} onChange={handleInputChange} /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="brwPressure">Back Right Wheel Pressure</Label><Input id="brwPressure" type="number" value={newEventState.brwPressure || ''} onChange={handleInputChange} /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="blwPressure">Back Left Wheel Pressure</Label><Input id="blwPressure" type="number" value={newEventState.blwPressure || ''} onChange={handleInputChange} /></div>
            </div>
            <div className="mt-6 flex space-x-4">
              <div className="flex items-center space-x-2"><Checkbox id="frontWingOn" checked={newEventState.frontWingOn} onCheckedChange={(c) => handleCheckboxChange('frontWingOn', c as boolean)} /><Label htmlFor="frontWingOn">Front Wing On</Label></div>
              <div className="flex items-center space-x-2"><Checkbox id="rearWingOn" checked={newEventState.rearWingOn} onCheckedChange={(c) => handleCheckboxChange('rearWingOn', c as boolean)} /><Label htmlFor="rearWingOn">Rear Wing On</Label></div>
              <div className="flex items-center space-x-2"><Checkbox id="regenOn" checked={newEventState.regenOn} onCheckedChange={(c) => handleCheckboxChange('regenOn', c as boolean)} /><Label htmlFor="regenOn">Regen On</Label></div>
              <div className="flex items-center space-x-2"><Checkbox id="undertrayOn" checked={newEventState.undertrayOn} onCheckedChange={(c) => handleCheckboxChange('undertrayOn', c as boolean)} /><Label htmlFor="undertrayOn">Undertray On</Label></div>
            </div>
            <div className="mt-6 flex justify-end">
              <Button type="button" onClick={handleCreateEvent}>Create Event</Button>
            </div>
          </form>
        </CardContent>
      </Card>
    </div>
  );
}
