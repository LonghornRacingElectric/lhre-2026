
'use client';

import { useEffect, useState } from 'react';
import { usePathname, useRouter } from 'next/navigation';
import { AppState } from '@/lib/types';

export const PageSync = () => {
  const router = useRouter();
  const pathname = usePathname();
  const [appState, setAppState] = useState<AppState>({});

  useEffect(() => {
    const eventSource = new EventSource('/api/event-sync');
    eventSource.onmessage = (event) => {
      const newState: AppState = JSON.parse(event.data);
      setAppState(newState);
      if (newState.currentPage && newState.currentPage !== pathname) {
        router.push(newState.currentPage);
      }
    };
    return () => eventSource.close();
  }, [pathname, router]);

  return null;
};
