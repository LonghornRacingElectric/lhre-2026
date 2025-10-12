'use client';

import { usePathname } from 'next/navigation';
import Banner from '@/components/Banner';

export default function PageLayout({ children }) {
  const pathname = usePathname();
  const showBanner = pathname !== '/live-viewer';

  return (
    <>
      {showBanner && <Banner />}
      <main>
        {children}
      </main>
    </>
  );
}
