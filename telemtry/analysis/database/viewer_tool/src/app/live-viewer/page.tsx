'use client';

import React, { useState, useEffect } from 'react';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { AppState } from '@/lib/types';

const LiveViewerPage = () => {
  const [appState, setAppState] = useState<AppState>({});

  useEffect(() => {
    const eventSource = new EventSource('/api/event-sync');
    eventSource.onmessage = (event) => {
      const newState: AppState = JSON.parse(event.data);
      setAppState(newState);
    };
    return () => eventSource.close();
  }, []);

  const features = [
    'Lap Timer',
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
                  <Button style={{ backgroundColor: 'yellow', color: 'black' }}>Hit Cone</Button>
                  <Button variant="destructive">Off-track</Button>
                  <Button variant="outline">Other Flag</Button>
                </div>
              )}
            </CardContent>
          </Card>
        ))}
      </div>
    </div>
  );
};

export default LiveViewerPage;
