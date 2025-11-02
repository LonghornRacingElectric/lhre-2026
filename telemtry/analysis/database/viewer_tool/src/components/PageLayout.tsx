'use client';

import { usePathname } from 'next/navigation';
import Banner from '@/components/Banner';
import { useEffect } from 'react';

export default function PageLayout({ children }) {
  const pathname = usePathname();
  const showBanner = pathname !== '/live-viewer';

  useEffect(() => {
    if (pathname === '/') {
      document.title = 'Telemtry | Home';
    } else if (pathname.startsWith('/driveday')) {
        document.title = 'Telemtry | Create Driveday';
    } else if (pathname.startsWith('/event/new')) {
        document.title = 'Telemtry | Create Event';
    } else if (pathname.startsWith('/tune')) {
      document.title = 'Telemtry | Texas Tune';
    } else if (pathname.startsWith('/live-viewer')) {
      document.title = 'Telemtry | Live Viewer';
    } else {
      document.title = 'Telemtry';
    }
  }, [pathname]);

  return (
    <>
      {showBanner && <Banner />}
      <main>
        {children}
      </main>
    </>
  );
}
