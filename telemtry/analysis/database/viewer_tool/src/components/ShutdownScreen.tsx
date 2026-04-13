'use client';

import { useKafkaJSON } from '@/hooks/useKafkaStream';
import { useCarSelection } from '@/lib/carSelection';

type ShutdownData = {
  shutdownHealthy?: boolean;
  contactorState?: number;
  hvcStateMachine?: number;
  shutdownCurrent?: number;
  r2dAuthorized?: boolean;
  r2dStatus?: boolean;
  negHvContactor?: boolean;
  posHvContactor?: boolean;
  prechargeContactor?: boolean;
  legs?: Record<string, boolean | undefined>;
};

const StatusChip = ({
  label,
  value,
}: {
  label: string;
  value: boolean | null | undefined;
}) => {
  const colorClass =
    value === true ? 'text-green-600' : value === false ? 'text-red-600' : 'text-gray-500';
  const labelText = value === true ? 'OK' : value === false ? 'Fault' : '—';

  return (
    <div className="border rounded p-2 bg-white">
      <div className="text-xs text-gray-500">{label}</div>
      <div className={`font-semibold ${colorClass}`}>{labelText}</div>
    </div>
  );
};

const ShutdownScreen = () => {
  const { selectedCar, selectedCarLabel, ssePath, matchesSelectedCar } = useCarSelection();
  const { data } = useKafkaJSON<ShutdownData>({
    topic: 'shutdown_screen',
    car: selectedCar,
    ssePath,
    filter: matchesSelectedCar,
    staleAfterMs: 2000,
    sampleMs: 120,
    merge: true,
  });

  const legs = Object.entries(data?.legs ?? {}).filter(
    ([, value]) => typeof value === 'boolean',
  ) as Array<[string, boolean]>;
  const hasCircuitData =
    legs.length > 0 ||
    typeof data?.negHvContactor === 'boolean' ||
    typeof data?.posHvContactor === 'boolean' ||
    typeof data?.prechargeContactor === 'boolean' ||
    typeof data?.r2dStatus === 'boolean';

  return (
    <div className="h-full w-full flex flex-col px-4 py-2 text-sm">
      <h3 className="text-lg font-semibold mb-2">{selectedCarLabel} Shutdown Circuit</h3>

      <div className="grid grid-cols-2 md:grid-cols-4 gap-2 mb-3">
        <StatusChip
          label="Circuit Health"
          value={data?.shutdownHealthy ?? (hasCircuitData ? false : null)}
        />
        <StatusChip label="R2D Authorized" value={data?.r2dAuthorized} />
        <StatusChip label="R2D Status" value={data?.r2dStatus} />
        <StatusChip label="Precharge" value={data?.prechargeContactor} />
      </div>

      <div className="grid grid-cols-2 md:grid-cols-4 gap-2 mb-3">
        <StatusChip label="Negative HV Contactor" value={data?.negHvContactor} />
        <StatusChip label="Positive HV Contactor" value={data?.posHvContactor} />
        <div className="border rounded p-2 bg-white">
          <div className="text-xs text-gray-500">Shutdown Current</div>
          <div className="font-semibold">
            {typeof data?.shutdownCurrent === 'number'
              ? `${data.shutdownCurrent.toFixed(2)} A`
              : '—'}
          </div>
        </div>
        <div className="border rounded p-2 bg-white">
          <div className="text-xs text-gray-500">HVC State</div>
          <div className="font-semibold">
            {typeof data?.hvcStateMachine === 'number' ? data.hvcStateMachine : '—'}
          </div>
        </div>
      </div>

      <div className="border rounded p-2 bg-white mb-3 max-w-[12rem]">
        <div className="text-xs text-gray-500">Contactor State</div>
        <div className="font-semibold">
          {typeof data?.contactorState === 'number' ? data.contactorState : '—'}
        </div>
      </div>

      {legs.length ? (
        <div className="grid grid-cols-2 md:grid-cols-4 gap-2">
          {legs.map(([leg, value]) => (
            <StatusChip key={leg} label={leg.replace('leg', 'Leg ')} value={value} />
          ))}
        </div>
      ) : (
        <p className="text-xs text-gray-500">
          No per-leg shutdown telemetry available for {selectedCarLabel} in current stream.
        </p>
      )}
    </div>
  );
};

export default ShutdownScreen;
