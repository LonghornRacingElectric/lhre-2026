
'use client';

import { useState, useEffect } from 'react';
import Link from 'next/link';
import { AppState } from '@/lib/types';

const SplashBox = ({ href, title, bgColor }: { href: string; title: string; bgColor: string }) => (
  <Link href={href}>
    <div className={`relative flex items-center justify-center h-full rounded-lg shadow-lg cursor-pointer transition-transform hover:scale-105 ${bgColor}`}>
      <div className="absolute inset-0 bg-black bg-opacity-40 rounded-lg"></div>
      <span className="relative text-white text-3xl font-bold text-center">{title}</span>
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
    if (appState.eventTracker?.isTimerRunning) {
      return '/event/tracker';
    }
    if (appState.newEvent) {
      return '/event/tracker';
    }
    if (appState.driveDay) {
      return '/event/new';
    }
    return '/driveday';
  };

  return (
    <div className="h-screen w-screen p-8 pt-20">
      <div className="grid grid-cols-1 md:grid-cols-3 gap-8 h-full">
        <SplashBox href={getDrivedayHref()} title="Driveday Page" bgColor="bg-blue-500" />
        <SplashBox href="/tune" title="Texas Tune" bgColor="bg-orange-500" />
        <SplashBox href="/dashboards" title="Grafana & Database" bgColor="bg-green-500" />
      </div>
    </div>
  );
}
