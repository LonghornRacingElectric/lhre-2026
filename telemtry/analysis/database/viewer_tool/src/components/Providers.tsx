
'use client';

import { SessionProvider } from 'next-auth/react';
import { CarSelectionProvider } from '@/lib/carSelection';

export default function Providers({ children }: { children: React.ReactNode }) {
  return (
    <SessionProvider>
      <CarSelectionProvider>{children}</CarSelectionProvider>
    </SessionProvider>
  );
}
