'use client';

import { useEffect, useRef, useState } from 'react';

import { useKafkaJSON } from '@/hooks/useKafkaStream';
import { useCarSelection } from '@/lib/carSelection';

type TimingData = {
  speed?: number | null;
  throttlePct?: number | null;
  brakePct?: number | null;
  packetId?: number | null;
};

const HISTORY_LIMIT = 120;

function toFiniteNumber(value: number | null | undefined): number | undefined {
  if (typeof value !== 'number' || !Number.isFinite(value)) return undefined;
  return value;
}

const TimingDeltas = () => {
  const { selectedCar, selectedCarLabel, ssePath, matchesSelectedCar } = useCarSelection();
  const [speedHistory, setSpeedHistory] = useState<number[]>([]);
  const [sampleRateHz, setSampleRateHz] = useState<number | undefined>(undefined);
  const lastSampleAtRef = useRef<number | undefined>(undefined);

  const { data } = useKafkaJSON<TimingData>({
    topic: 'timing_deltas',
    car: selectedCar,
    ssePath,
    filter: matchesSelectedCar,
    staleAfterMs: 2000,
    merge: true,
    onMessage: (_evt, parsed) => {
      const speed = toFiniteNumber(parsed?.speed);
      if (speed !== undefined) {
        setSpeedHistory((prev) => [...prev.slice(-(HISTORY_LIMIT - 1)), speed]);
      }

      const now = Date.now();
      if (lastSampleAtRef.current) {
        const hz = 1000 / (now - lastSampleAtRef.current);
        if (Number.isFinite(hz)) {
          setSampleRateHz(hz);
        }
      }
      lastSampleAtRef.current = now;
    },
  });

  useEffect(() => {
    setSpeedHistory([]);
    setSampleRateHz(undefined);
    lastSampleAtRef.current = undefined;
  }, [selectedCar]);

  const currentSpeed = toFiniteNumber(data?.speed);
  const rollingSpeed =
    speedHistory.length > 0
      ? speedHistory.reduce((sum, value) => sum + value, 0) / speedHistory.length
      : undefined;
  const paceDelta =
    currentSpeed !== undefined && rollingSpeed !== undefined
      ? currentSpeed - rollingSpeed
      : undefined;
  const paceDeltaClass =
    paceDelta === undefined
      ? 'text-gray-600'
      : paceDelta >= 0
      ? 'text-green-600'
      : 'text-red-600';

  return (
    <div className="h-full w-full flex flex-col px-4 py-2 text-sm">
      <h3 className="text-lg font-semibold mb-2">{selectedCarLabel} Timing &amp; Deltas</h3>
      <p className="text-xs text-gray-500 mb-3">
        Live pacing is calculated from telemetry speed against a rolling baseline.
      </p>

      <div className="grid grid-cols-2 md:grid-cols-4 gap-3">
        <div className="border rounded p-3 bg-white">
          <div className="text-gray-500">Current Speed</div>
          <div className="font-semibold">
            {currentSpeed !== undefined ? currentSpeed.toFixed(2) : '—'}
          </div>
        </div>
        <div className="border rounded p-3 bg-white">
          <div className="text-gray-500">Rolling Pace</div>
          <div className="font-semibold">
            {rollingSpeed !== undefined ? rollingSpeed.toFixed(2) : '—'}
          </div>
        </div>
        <div className="border rounded p-3 bg-white">
          <div className="text-gray-500">Pace Delta</div>
          <div className={`font-semibold ${paceDeltaClass}`}>
            {paceDelta !== undefined
              ? `${paceDelta >= 0 ? '+' : ''}${paceDelta.toFixed(2)}`
              : '—'}
          </div>
        </div>
        <div className="border rounded p-3 bg-white">
          <div className="text-gray-500">Sample Rate</div>
          <div className="font-semibold">
            {sampleRateHz !== undefined ? `${sampleRateHz.toFixed(1)} Hz` : '—'}
          </div>
        </div>
      </div>
    </div>
  );
};

export default TimingDeltas;
