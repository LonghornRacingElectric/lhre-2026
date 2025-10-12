
'use client';

import { PageSync } from '@/components/PageSync';

export default function DrivedayLayout({ children }: { children: React.ReactNode }) {
  return (
    <>
      <PageSync />
      {children}
    </>
  );
}
