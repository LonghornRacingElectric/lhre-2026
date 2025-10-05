'use client';

import React, { useState, useEffect, useRef } from 'react';
import dynamic from 'next/dynamic';
import {
  DndContext,
  closestCenter,
  KeyboardSensor,
  PointerSensor,
  useSensor,
  useSensors,
} from '@dnd-kit/core';
import {
  arrayMove,
  SortableContext,
  sortableKeyboardCoordinates,
  rectSortingStrategy,
} from '@dnd-kit/sortable';
import screenfull from 'screenfull';

import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { AppState } from '@/lib/types';
import LapTimer from '@/components/LapTimer';
import { useSortableTile } from '@/hooks/useSortableTile';

const DynamicMap = dynamic(() => import('@/components/Map'), {
  ssr: false,
});

const Tile = ({ feature, appState, note, setNote, handleSubmitNote, isDragging }) => {
    const { attributes, listeners, setNodeRef, style } = useSortableTile(feature.id);
    const cardRef = useRef<HTMLDivElement>(null);

    const handleFullscreen = () => {
        if (screenfull.isEnabled && cardRef.current) {
            screenfull.toggle(cardRef.current);
        }
    };

    const renderFeature = (feature) => {
        if (isDragging && feature.id === 'live-map') {
            return <div className="w-full h-full bg-gray-200 flex items-center justify-center">Map is hidden during drag</div>;
        }

        switch (feature.id) {
            case 'lap-timer':
                return <LapTimer />;
            case 'flagging-inputs':
                return (
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
                );
            case '3d-simulation':
                return (
                    <img
                      src="/window.svg"
                      alt="3D Simulation"
                      className="w-full h-full object-contain"
                    />
                );
            case 'space-time-trajectory':
                return (
                    <img
                      key={appState.liveImage || 'no-image'}
                      id="live-image"
                      src={appState.liveImage ? `data:image/png;base64,${appState.liveImage}` : '/images/events.png'}
                      alt="Live Data"
                      className="w-full h-full object-contain"
                    />
                );
            case 'live-map':
                return <DynamicMap />;
            case 'gg-plot':
                return (
                    <img
                      src="/window.svg"
                      alt="GG Plot"
                      className="w-full h-full object-contain"
                    />
                );
            default:
                return null;
        }
    }

    return (
        <div ref={setNodeRef} style={style}>
            <Card ref={cardRef} className="h-full flex flex-col">
                <CardHeader className="flex flex-row justify-between items-center">
                    <CardTitle>{feature.name}</CardTitle>
                    <div className="flex items-center">
                        <Button variant="ghost" size="icon" onClick={handleFullscreen}>
                            <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M8 3H5a2 2 0 0 0-2 2v3m18 0V5a2 2 0 0 0-2-2h-3m0 18h3a2 2 0 0 0 2-2v-3M3 16v3a2 2 0 0 0 2 2h3"/></svg>
                        </Button>
                        <Button {...attributes} {...listeners} variant="ghost" size="icon" className="cursor-grab">
                            <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><circle cx="12" cy="12" r="1"></circle><circle cx="12" cy="5" r="1"></circle><circle cx="12" cy="19" r="1"></circle></svg>
                        </Button>
                    </div>
                </CardHeader>
                <CardContent className="flex-grow">
                    {renderFeature(feature)}
                </CardContent>
            </Card>
        </div>
    )
}

const LiveViewerPage = () => {
  const [appState, setAppState] = useState<AppState>({});
  const [note, setNote] = useState('');
  const [features, setFeatures] = useState([
    { id: 'lap-timer', name: 'Lap Timer' },
    { id: 'flagging-inputs', name: 'Flagging Inputs' },
    { id: '3d-simulation', name: '3D Simulation' },
    { id: 'space-time-trajectory', name: 'Space-Time Trajectory' },
    { id: 'live-map', name: 'Live Map' },
    { id: 'gg-plot', name: 'GG Plot' },
  ]);
  const [isDragging, setIsDragging] = useState(false);
  const [isClient, setIsClient] = useState(false);

  useEffect(() => {
    setIsClient(true);
    const storedFeatures = localStorage.getItem('featuresOrder');
    if (storedFeatures) {
      setFeatures(JSON.parse(storedFeatures));
    }
  }, []);

  const sensors = useSensors(
    useSensor(PointerSensor),
    useSensor(KeyboardSensor, {
      coordinateGetter: sortableKeyboardCoordinates,
    })
  );

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

  const handleDragStart = () => {
    setIsDragging(true);
  }

  const handleDragEnd = (event) => {
    setIsDragging(false);
    const { active, over } = event;

    if (active.id !== over.id) {
      setFeatures((items) => {
        const oldIndex = items.findIndex((item) => item.id === active.id);
        const newIndex = items.findIndex((item) => item.id === over.id);
        const newItems = arrayMove(items, oldIndex, newIndex);
        localStorage.setItem('featuresOrder', JSON.stringify(newItems));
        return newItems;
      });
    }
  };

  if (!isClient) {
    return null; // or a loading spinner
  }

  return (
    <div className="container mx-auto p-8">
      <h1 className="text-4xl font-bold mb-8 text-center">Live Viewer</h1>
      <DndContext
        sensors={sensors}
        collisionDetection={closestCenter}
        onDragStart={handleDragStart}
        onDragEnd={handleDragEnd}
      >
        <SortableContext items={features.map(f => f.id)} strategy={rectSortingStrategy}>
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-8">
            {features.map((feature) => (
              <Tile key={feature.id} feature={feature} appState={appState} note={note} setNote={setNote} handleSubmitNote={handleSubmitNote} isDragging={isDragging} />
            ))}
          </div>
        </SortableContext>
      </DndContext>
    </div>
  );
};

export default LiveViewerPage;