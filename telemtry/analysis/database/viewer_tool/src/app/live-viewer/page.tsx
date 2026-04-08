"use client";

import React, { useState, useEffect, useRef } from "react";
import Image from "next/image";
import dynamic from "next/dynamic";
import {
  DndContext,
  closestCenter,
  KeyboardSensor,
  PointerSensor,
  useSensor,
  useSensors,
} from "@dnd-kit/core";
import {
  arrayMove,
  SortableContext,
  sortableKeyboardCoordinates,
  rectSortingStrategy,
} from "@dnd-kit/sortable";
import screenfull from "screenfull";

import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { AppState } from "@/lib/types";
import TimingDeltas from "@/components/TimingDeltas";
import LiveViewerBanner from "@/components/LiveViewerBanner";
import { useSortableTile } from "@/hooks/useSortableTile";
import CarVisualization from "@/components/CarVisualization";
import DriverInputVisualizer from "@/components/DriverInputVisualizer";
import GGPlot from "@/components/GGPlot";
import DashboardScreen from "@/components/DashboardScreen";
import ShutdownScreen from "@/components/ShutdownScreen";
import EventFlagger from "@/components/EventFlagger";
import ThermalHeadroomMeter from "@/components/ThermalHeadroomMeter";
import EnergyBudget from "@/components/EnergyBudget";

const DynamicMap = dynamic(() => import("@/components/Map"), {
  ssr: false,
});

const Tile = ({ feature, appState, note, setNote, isDragging }) => {
  const { attributes, listeners, setNodeRef, style } = useSortableTile(
    feature.id
  );
  const cardRef = useRef<HTMLDivElement>(null);
  const [renderKey, setRenderKey] = useState(0);
  const [isFullscreen, setIsFullscreen] = useState(false);

  useEffect(() => {
    const handleScreenfullChange = () => {
      setIsFullscreen(screenfull.isFullscreen);
      if (!screenfull.isFullscreen) {
        setRenderKey(prevKey => prevKey + 1);
      }
    };

    if (screenfull.isEnabled) {
      screenfull.on('change', handleScreenfullChange);
    }

    return () => {
      if (screenfull.isEnabled) {
        screenfull.off('change', handleScreenfullChange);
      }
    };
  }, []);

  const handleFullscreen = () => {
    if (screenfull.isEnabled && cardRef.current) {
      screenfull.toggle(cardRef.current);
    }
  };

  const renderFeature = (feature) => {
    if (isDragging && feature.id === "live-map") {
      return (
        <div className="w-full h-full bg-gray-200 flex items-center justify-center">
          Map is hidden during drag
        </div>
      );
    }

    switch (feature.id) {
      case "lap-timer":
        return <TimingDeltas />;
      case "flagging-inputs":
        return (
          <EventFlagger
            note={note}
            setNote={setNote}
          />
        );
      case "3d-simulation":
        return (
          <div className="w-full h-full object-contain">
            <CarVisualization key={renderKey} />
          </div>
        );
      case "live-map":
        return (
          <div className="w-full" style={{ height: isFullscreen ? '100%' : '350px' }}>
            <DynamicMap key={renderKey} resize={renderKey} />
          </div>
        );
      case "gg-plot":
        return <GGPlot />;
      case "driver-input":
        return <DriverInputVisualizer />;
      case "car-dashboard":
        return (
          <div className="w-full h-full">
            <DashboardScreen />
          </div>
        );
      case "shutdown-screen":
        return (
          <div className="w-full h-full">
            <ShutdownScreen />
          </div>
        );
      case "thermal-headroom":
        return (
          <div className="w-full h-full">
            <ThermalHeadroomMeter />
          </div>
        );
      case "energy-budget":
        return (
          <div className="w-full h-full">
            <EnergyBudget />
          </div>
        );
      default:
        return null;
    }
  };

  return (
    <div ref={setNodeRef} style={style}>
      <Card ref={cardRef} className="h-full flex flex-col">
        <CardHeader className="flex flex-row justify-between items-center">
          <CardTitle>{feature.name}</CardTitle>
          <div className="flex items-center">
            <Button variant="ghost" size="icon" onClick={handleFullscreen}>
              <svg
                xmlns="http://www.w3.org/2000/svg"
                width="24"
                height="24"
                viewBox="0 0 24 24"
                fill="none"
                stroke="currentColor"
                strokeWidth="2"
                strokeLinecap="round"
                strokeLinejoin="round"
              >
                <path d="M8 3H5a2 2 0 0 0-2 2v3m18 0V5a2 2 0 0 0-2-2h-3m0 18h3a2 2 0 0 0 2-2v-3M3 16v3a2 2 0 0 0 2 2h3" />
              </svg>
            </Button>
            <Button
              {...attributes}
              {...listeners}
              variant="ghost"
              size="icon"
              className="cursor-grab"
            >
              <svg
                xmlns="http://www.w3.org/2000/svg"
                width="24"
                height="24"
                viewBox="0 0 24 24"
                fill="none"
                stroke="currentColor"
                strokeWidth="2"
                strokeLinecap="round"
                strokeLinejoin="round"
              >
                <circle cx="12" cy="12" r="1"></circle>
                <circle cx="12" cy="5" r="1"></circle>
                <circle cx="12" cy="19" r="1"></circle>
              </svg>
            </Button>
          </div>
        </CardHeader>
        <CardContent className="flex-grow">
          {renderFeature(feature)}
        </CardContent>
      </Card>
    </div>
  );
};

const LiveViewerPage = () => {
  const [appState, setAppState] = useState<AppState>({});
  const [note, setNote] = useState("");
  const [features, setFeatures] = useState([
    { id: "lap-timer", name: "Timing & Deltas" },
    { id: "flagging-inputs", name: "Flagging Events" },
    { id: "3d-simulation", name: "3D Simulation" },
    { id: "live-map", name: "Live Map" },
    { id: "gg-plot", name: "GG Plot" },
    { id: "thermal-headroom", name: "Thermal Headroom Meter" },
    { id: "driver-input", name: "Driver Input Visualizer" },
    { id: "energy-budget", name: "Energy Budget & Predictive SOC" },
    { id: "car-dashboard", name: "Car Dashboard" },
    { id: "shutdown-screen", name: "Shutdown Circuit Status" },
  ]);
  const [isDragging, setIsDragging] = useState(false);
  const [isClient, setIsClient] = useState(false);

  useEffect(() => {
    setIsClient(true);
    const storedFeatures = localStorage.getItem("featuresOrder");
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
    const eventSource = new EventSource("/api/event-sync");
    eventSource.onmessage = (event) => {
      const newState: AppState = JSON.parse(event.data);
      setAppState(newState);
    };
    return () => eventSource.close();
  }, []);

  const handleDragStart = () => {
    setIsDragging(true);
  };

  const handleDragEnd = (event) => {
    setIsDragging(false);
    const { active, over } = event;

    if (active.id !== over.id) {
      setFeatures((items) => {
        const oldIndex = items.findIndex((item) => item.id === active.id);
        const newIndex = items.findIndex((item) => item.id === over.id);
        const newItems = arrayMove(items, oldIndex, newIndex);
        localStorage.setItem("featuresOrder", JSON.stringify(newItems));
        return newItems;
      });
    }
  };

  if (!isClient) {
    return null; // or a loading spinner
  }

  return (
    <>
      <LiveViewerBanner />
      <div className="container mx-auto p-8 pt-24 md:pt-20">
        <DndContext
          sensors={sensors}
          collisionDetection={closestCenter}
          onDragStart={handleDragStart}
          onDragEnd={handleDragEnd}
        >
          <SortableContext
            items={features.map((f) => f.id)}
            strategy={rectSortingStrategy}
          >
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-8 grid-auto-rows-[400px]">
              {features.map((feature) => (
                <Tile
                  key={feature.id}
                  feature={feature}
                  appState={appState}
                  note={note}
                  setNote={setNote}
                  isDragging={isDragging}
                />
              ))}
            </div>
          </SortableContext>
        </DndContext>
      </div>
    </>
  );
};

export default LiveViewerPage;
