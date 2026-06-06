'use client';

import Link from 'next/link';

const SplashBox = ({ href, title, subtitle, gradient }: { href: string; title: string; subtitle?: string; gradient: string }) => (
  <Link href={href} className="flex-grow">
    <div className={`relative flex flex-col items-center justify-center h-full min-h-[200px] rounded-lg shadow-lg cursor-pointer overflow-hidden group transition-colors ${gradient}`}>
      <span className="relative text-3xl font-bold text-center z-20 text-white">{title}</span>
      {subtitle && <span className="relative mt-2 text-sm text-center z-20 text-white/70 px-6">{subtitle}</span>}
    </div>
  </Link>
);

export default function SplashPage() {
  return (
    <div className="min-h-screen flex flex-col justify-between pt-14">
      <div className="grid grid-cols-1 md:grid-cols-2 gap-8 flex-grow p-8">
        <SplashBox
          href="/trackside-live"
          title="Trackside Live"
          subtitle="Live lap timing, energy & dash comms"
          gradient="bg-gradient-to-br from-teal-700 to-emerald-900 group-hover:from-teal-600 group-hover:to-emerald-800"
        />
        <SplashBox
          href="/trackside-live?focus=car"
          title="Car Status"
          subtitle="Is the car online? Orion / BEVO health"
          gradient="bg-gradient-to-br from-sky-700 to-blue-900 group-hover:from-sky-600 group-hover:to-blue-800"
        />
        <SplashBox
          href="/log-sync"
          title="Log Sync"
          subtitle="Pull & annotate CSV logs from the car"
          gradient="bg-gradient-to-br from-slate-700 to-slate-900 group-hover:from-slate-600 group-hover:to-slate-800"
        />
        <SplashBox
          href="/dashboards"
          title="Grafana"
          subtitle="Live dashboards & historical database"
          gradient="bg-gradient-to-br from-orange-700 to-amber-900 group-hover:from-orange-600 group-hover:to-amber-800"
        />
      </div>
    </div>
  );
}
