'use client';

import { useState, useEffect } from 'react';
import Link from 'next/link';
import Image from 'next/image';
import { AppState } from '@/lib/types';

const SplashBox = ({ href, title, imageUrl, textColor }: { href: string; title: string; imageUrl: string; textColor?: string }) => (
  <Link href={href} className="flex-grow">
    <div className={`relative flex items-center justify-center h-full rounded-lg shadow-lg cursor-pointer overflow-hidden group`}>
      <Image src={imageUrl} alt={title} fill style={{ objectFit: "cover" }} className="transition-transform duration-300 group-hover:scale-105" />
      <span className={`relative text-3xl font-bold text-center z-20 ${textColor || 'text-white'}`}>{title}</span>
    </div>
  </Link>
);

export default function SplashPage() {
  const [appState, setAppState] = useState<AppState>({});

  useEffect(() => {
    const eventSource = new EventSource('/api/event-sync');
    eventSource.onmessage = (event) => {
      const newState: AppState = JSON.parse(event.data);
      setAppState(newState);
    };
    return () => eventSource.close();
  }, []);

  const getDrivedayHref = () => {
    if (appState.currentPage) {
      return appState.currentPage;
    }
    if (appState.eventTracker?.isTimerRunning) {
      return '/event/in-progress';
    }
    if (appState.newEvent) {
      return '/event/in-progress';
    }
    if (appState.driveDay) {
      return '/event/new';
    }
    return '/driveday';
  };

  return (
    <div className="min-h-screen flex flex-col justify-between pt-14">
      <div className="grid grid-cols-1 md:grid-cols-2 gap-8 flex-grow p-8">
        <SplashBox href={getDrivedayHref()} title="Driveday Page" imageUrl="/car.png" />
        <SplashBox href="/live-viewer" title="Live Viewer" imageUrl="/images/live.png" textColor="text-black" />
        <SplashBox href="/replay" title="Replay Mode" imageUrl="/images/replay.png" textColor="text-black" />
        <SplashBox href="/tune" title="Texas Tune" imageUrl="/tune.png" textColor="text-black" />
        <SplashBox href="/dashboards" title="Grafana & Database" imageUrl="/graph.png" />
      </div>
    </div>
  );
}