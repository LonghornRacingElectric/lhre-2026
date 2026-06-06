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
          gradient="bg-gradient-to-br from-[#d97757] to-[#7a3a26] group-hover:from-[#e0876a] group-hover:to-[#8c452e]"
        />
        <SplashBox
          href="/trackside-live?focus=car"
          title="Car Status"
          subtitle="Is the car online? Orion / BEVO health"
          gradient="bg-gradient-to-br from-[#2f5fb0] to-[#16243d] group-hover:from-[#3a6ec5] group-hover:to-[#1c2e4d]"
        />
        <SplashBox
          href="/log-sync"
          title="Log Sync"
          subtitle="Pull & annotate CSV logs from the car"
          gradient="bg-gradient-to-br from-[#33312e] to-[#161514] group-hover:from-[#42403c] group-hover:to-[#1d1c1a]"
        />
        <SplashBox
          href="/dashboards"
          title="Grafana"
          subtitle="Live dashboards & historical database"
          gradient="bg-gradient-to-br from-[#b07712] to-[#3a2a08] group-hover:from-[#c98916] group-hover:to-[#4a360c]"
        />
      </div>
    </div>
  );
}
