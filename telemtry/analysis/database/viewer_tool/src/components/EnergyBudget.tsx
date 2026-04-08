'use client';

import { useKafkaJSON } from '@/hooks/useKafkaStream';
import { useCarSelection } from '@/lib/carSelection';

type EnergyBudgetData = {
  batteryPct?: number | null;
  hvPackV?: number | null;
  hvCurrent?: number | null;
  powerKw?: number | null;
  lvV?: number | null;
  timeSinceOnS?: number | null;
};

function formatNumber(value: number | null | undefined, digits = 1): string {
  if (typeof value !== 'number' || !Number.isFinite(value)) return '—';
  return value.toFixed(digits);
}

export default function EnergyBudget() {
  const { selectedCar, selectedCarLabel, ssePath, matchesSelectedCar } = useCarSelection();
  const { data } = useKafkaJSON<EnergyBudgetData>({
    topic: 'energy_budget',
    car: selectedCar,
    ssePath,
    filter: matchesSelectedCar,
    staleAfterMs: 2000,
    merge: true,
  });

  const powerKw =
    typeof data?.powerKw === 'number' && Number.isFinite(data.powerKw)
      ? data.powerKw
      : undefined;
  const powerMode =
    powerKw === undefined ? 'Unknown' : powerKw >= 0 ? 'Discharge' : 'Regen';
  const powerModeClass =
    powerKw === undefined
      ? 'text-gray-600'
      : powerKw >= 0
      ? 'text-orange-600'
      : 'text-green-600';

  return (
    <div className="h-full w-full flex flex-col px-4 py-2 text-sm">
      <h3 className="text-lg font-semibold mb-2">{selectedCarLabel} Energy Budget</h3>

      <div className="grid grid-cols-2 md:grid-cols-3 gap-2 mb-3">
        <div className="border rounded p-2 bg-white">
          <div className="text-xs text-gray-500">SOC</div>
          <div className="font-semibold">
            {typeof data?.batteryPct === 'number' ? `${Math.round(data.batteryPct)}%` : '—'}
          </div>
        </div>
        <div className="border rounded p-2 bg-white">
          <div className="text-xs text-gray-500">HV Voltage</div>
          <div className="font-semibold">{formatNumber(data?.hvPackV)} V</div>
        </div>
        <div className="border rounded p-2 bg-white">
          <div className="text-xs text-gray-500">HV Current</div>
          <div className="font-semibold">{formatNumber(data?.hvCurrent)} A</div>
        </div>
        <div className="border rounded p-2 bg-white">
          <div className="text-xs text-gray-500">Pack Power</div>
          <div className="font-semibold">
            {powerKw !== undefined ? `${powerKw.toFixed(1)} kW` : '—'}
          </div>
        </div>
        <div className="border rounded p-2 bg-white">
          <div className="text-xs text-gray-500">Mode</div>
          <div className={`font-semibold ${powerModeClass}`}>{powerMode}</div>
        </div>
        <div className="border rounded p-2 bg-white">
          <div className="text-xs text-gray-500">LV Voltage</div>
          <div className="font-semibold">{formatNumber(data?.lvV)} V</div>
        </div>
      </div>

      <p className="text-xs text-gray-500">
        Runtime telemetry from pack voltage/current and SOC. Long-horizon energy prediction remains
        future work.
      </p>
    </div>
  );
}
