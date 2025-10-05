'use client';

import React, { useState, useEffect } from 'react';
import dynamic from 'next/dynamic';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { AppState } from '@/lib/types';
import LapTimer from '@/components/LapTimer';

const DynamicMap = dynamic(() => import('@/components/Map'), {
  ssr: false,
});

const LiveViewerPage = () => {
  const [appState, setAppState] = useState<AppState>({});
  const [note, setNote] = useState('');

  useEffect(() => {
    const eventSource = new EventSource('/api/event-sync');
    eventSource.onmessage = (event) => {
      const newState: AppState = JSON.parse(event.data);
      setAppState(newState);
    };
    return () => eventSource.close();
  }, []);

  const handleSubmitNote = () => {
    // Handle note submission logic here
    console.log('Note submitted:', note);
    setNote('');
  };

  const features = [
    'Flagging Inputs',
    '3D Simulation',
    'Space-Time Trajectory',
    'Live Map',
    'GG Plot',
  ];

  return (
    <div className="container mx-auto p-8">
      <h1 className="text-4xl font-bold mb-8 text-center">Live Viewer</h1>
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-8">
        <LapTimer />
        {features.map((feature) => (
          <Card key={feature}>
            <CardHeader>
              <CardTitle className="text-center">{feature}</CardTitle>
            </CardHeader>
            <CardContent className="h-48">
              {feature === 'Space-Time Trajectory' && (
                <img
                  key={appState.liveImage || 'no-image'}
                  id="live-image"
                  src={appState.liveImage ? `data:image/png;base64,${appState.liveImage}` : '/images/events.png'}
                  alt="Live Data"
                  className="w-full h-full object-contain"
                />
              )}
              {feature === 'Flagging Inputs' && (
                <div className="flex flex-col space-y-2">
                  <Button variant="outline" className="bg-yellow-400 text-black hover:bg-yellow-500">Hit Cone</Button>
                  <Button variant="outline" className="bg-red-500 text-white hover:bg-red-600">Off-track</Button>
                  <Button variant="outline" className="bg-orange-500 text-white hover:bg-orange-600">Mark Incomplete</Button>
                  <Button variant="outline">Other Flag</Button>
                  <div className="flex space-x-2 pt-2">
                    <Input type="text" placeholder="Enter a note..." value={note} onChange={(e) => setNote(e.target.value)} />
                    <Button onClick={handleSubmitNote}>Submit</Button>
                  </div>
                </div>
              )}
              {feature === 'Live Map' && (
                <DynamicMap />
              )}
              {feature === '3D Simulation' && (
                <img
                  src="/window.svg"
                  alt="3D Simulation"
                  className="w-full h-full object-contain"
                />
              )}
              {feature === 'GG Plot' && (
                <img
                  src="/window.svg"
                  alt="GG Plot"
                  className="w-full h-full object-contain"
                />
              )}
            </CardContent>
          </Card>
        ))}
      </div>
    </div>
  );
};

export default LiveViewerPage;
