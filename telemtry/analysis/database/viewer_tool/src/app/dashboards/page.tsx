
'use client';

import { useEffect } from 'react';

export default function DashboardsPage() {
  useEffect(() => {
    window.location.href = 'https://lhrelectric.org/grafana';
  }, []);

  return (
    <div className="p-8">
      <h1 className="text-2xl font-bold">Redirecting to Grafana...</h1>
    </div>
  );
}
