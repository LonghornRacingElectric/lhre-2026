'use client';

import { useKafkaJSON } from '@/hooks/useKafkaStream';
import { useCarSelection } from '@/lib/carSelection';

type ThermalHeadroomData = {
  thermalHeadroomC?: number | null;
  hottestTempC?: number | null;
  inverterTempC?: number | null;
  motorTempC?: number | null;
  battTempC?: number | null;
  ambientTempC?: number | null;
  coolantTempC?: number | null;
  fanSpeed?: number | null;
};

function formatNumber(value: number | null | undefined, digits = 1): string {
  if (typeof value !== 'number' || !Number.isFinite(value)) return '—';
  return value.toFixed(digits);
}

export default function ThermalHeadroomMeter() {
  const { selectedCar, selectedCarLabel, ssePath, matchesSelectedCar } = useCarSelection();
  const { data } = useKafkaJSON<ThermalHeadroomData>({
    topic: 'thermal_headroom',
    car: selectedCar,
    ssePath,
    filter: matchesSelectedCar,
    staleAfterMs: 2000,
    merge: true,
  });

  const headroom = typeof data?.thermalHeadroomC === 'number' ? data.thermalHeadroomC : undefined;
  const barPct =
    headroom !== undefined ? Math.max(0, Math.min(100, (headroom / 60) * 100)) : 0;
  const headroomTone =
    headroom === undefined
      ? 'text-gray-600'
      : headroom <= 0
      ? 'text-red-600'
      : headroom <= 10
      ? 'text-yellow-600'
      : 'text-green-600';
  const barColor =
    headroom === undefined
      ? 'bg-gray-400'
      : headroom <= 0
      ? 'bg-red-500'
      : headroom <= 10
      ? 'bg-yellow-500'
      : 'bg-green-500';

  return (
    <div className="h-full w-full flex flex-col px-4 py-2 text-sm">
      <h3 className="text-lg font-semibold mb-2">{selectedCarLabel} Thermal Headroom</h3>

      <div className="border rounded bg-white p-3 mb-3">
        <div className="flex items-center justify-between mb-1">
          <span className="text-gray-500">Headroom to 90&deg;C limit</span>
          <span className={`font-semibold ${headroomTone}`}>
            {headroom !== undefined ? `${headroom.toFixed(1)}°C` : '—'}
          </span>
        </div>
        <div className="w-full h-3 rounded bg-gray-200 overflow-hidden">
          <div className={`h-full ${barColor}`} style={{ width: `${barPct}%` }} />
        </div>
      </div>

      <div className="grid grid-cols-2 md:grid-cols-3 gap-2">
        <div className="border rounded p-2 bg-white">
          <div className="text-xs text-gray-500">Hottest</div>
          <div className="font-semibold">{formatNumber(data?.hottestTempC)}&deg;C</div>
        </div>
        <div className="border rounded p-2 bg-white">
          <div className="text-xs text-gray-500">Inverter</div>
          <div className="font-semibold">{formatNumber(data?.inverterTempC)}&deg;C</div>
        </div>
        <div className="border rounded p-2 bg-white">
          <div className="text-xs text-gray-500">Motor</div>
          <div className="font-semibold">{formatNumber(data?.motorTempC)}&deg;C</div>
        </div>
        <div className="border rounded p-2 bg-white">
          <div className="text-xs text-gray-500">Battery</div>
          <div className="font-semibold">{formatNumber(data?.battTempC)}&deg;C</div>
        </div>
        <div className="border rounded p-2 bg-white">
          <div className="text-xs text-gray-500">Ambient</div>
          <div className="font-semibold">{formatNumber(data?.ambientTempC)}&deg;C</div>
        </div>
        <div className="border rounded p-2 bg-white">
          <div className="text-xs text-gray-500">Coolant</div>
          <div className="font-semibold">{formatNumber(data?.coolantTempC)}&deg;C</div>
        </div>
        <div className="border rounded p-2 bg-white">
          <div className="text-xs text-gray-500">Fan Speed</div>
          <div className="font-semibold">
            {typeof data?.fanSpeed === 'number' && Number.isFinite(data.fanSpeed)
              ? `${Math.round(data.fanSpeed)} rpm`
              : '—'}
          </div>
        </div>
      </div>
    </div>
  );
}
