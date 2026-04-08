'use client';

import { useKafkaJSON } from '@/hooks/useKafkaStream';
import { useCarSelection } from '@/lib/carSelection';

type DashboardData = {
  speed?: number | null;
  wheelSpeedAvg?: number | null;
  steerColAngle?: number | null;
  throttlePct?: number | null;
  brakePct?: number | null;
  batteryPct?: number | null;
  hvPackV?: number | null;
  hvCurrent?: number | null;
  lvV?: number | null;
  inverterTempC?: number | null;
  motorTempC?: number | null;
  ambientTempC?: number | null;
};

function formatNumber(value: number | null | undefined, digits = 1): string {
  if (typeof value !== 'number' || !Number.isFinite(value)) return '—';
  return value.toFixed(digits);
}

const DashboardScreen = () => {
  const { selectedCar, selectedCarLabel, ssePath, matchesSelectedCar } = useCarSelection();
  const { data: status, kafkaConnected } = useKafkaJSON<DashboardData>({
    topic: 'dashboard_screen',
    car: selectedCar,
    ssePath,
    filter: matchesSelectedCar,
    staleAfterMs: 2000,
    merge: true,
  });

  return (
    <div className="h-full w-full flex flex-col items-center justify-center text-center px-4">
      <h3 className="text-lg font-semibold mb-2">{selectedCarLabel} Dashboard</h3>
      <p className="text-sm text-gray-600 mb-4">
        Live summary from normalized telemetry stream.
      </p>

      <div className="grid grid-cols-1 sm:grid-cols-3 gap-3 w-full max-w-xl text-sm">
        <div className="border rounded p-3 bg-white">
          <div className="text-gray-500">Connection</div>
          <div className={kafkaConnected ? 'text-green-600 font-semibold' : 'text-red-600 font-semibold'}>
            {kafkaConnected ? 'Live' : 'No recent data'}
          </div>
        </div>
        <div className="border rounded p-3 bg-white">
          <div className="text-gray-500">Battery</div>
          <div className="font-semibold">
            {typeof status?.batteryPct === 'number' ? `${Math.round(status.batteryPct)}%` : '—'}
          </div>
        </div>
        <div className="border rounded p-3 bg-white">
          <div className="text-gray-500">Speed</div>
          <div className="font-semibold">{formatNumber(status?.speed)}</div>
        </div>
        <div className="border rounded p-3 bg-white">
          <div className="text-gray-500">Steering</div>
          <div className="font-semibold">{formatNumber(status?.steerColAngle)}&deg;</div>
        </div>
        <div className="border rounded p-3 bg-white">
          <div className="text-gray-500">Throttle / Brake</div>
          <div className="font-semibold">
            {formatNumber(status?.throttlePct, 0)}% / {formatNumber(status?.brakePct, 0)}%
          </div>
        </div>
        <div className="border rounded p-3 bg-white">
          <div className="text-gray-500">HV Pack</div>
          <div className="font-semibold">{formatNumber(status?.hvPackV)} V</div>
        </div>
        <div className="border rounded p-3 bg-white">
          <div className="text-gray-500">HV Current</div>
          <div className="font-semibold">{formatNumber(status?.hvCurrent)} A</div>
        </div>
        <div className="border rounded p-3 bg-white">
          <div className="text-gray-500">Inverter / Motor</div>
          <div className="font-semibold">
            {formatNumber(status?.inverterTempC)}&deg;C / {formatNumber(status?.motorTempC)}&deg;C
          </div>
        </div>
        <div className="border rounded p-3 bg-white">
          <div className="text-gray-500">Ambient</div>
          <div className="font-semibold">
            {formatNumber(status?.ambientTempC)}&deg;C
          </div>
        </div>
      </div>
    </div>
  );
};

export default DashboardScreen;
