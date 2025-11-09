'use client';

import { useKafkaJSON } from '@/hooks/useKafkaStream';
import { useState } from 'react';

// TODO: Replace with actual steering wheel image maybe
const SteeringWheelIcon = () => (
  <svg width="40" height="40" viewBox="0 0 100 100">
    <circle cx="50" cy="50" r="45" stroke="black" strokeWidth="10" fill="none" />
    <line x1="50" y1="5" x2="50" y2="35" stroke="black" strokeWidth="10" />
  </svg>
);

const DriverInputVisualizer = () => {
  // Live connection to Kafka "sensor_data" topic
  const { data: sensorData, connected: kafkaConnected } = useKafkaJSON<{
    dynamics: { inverterC: number };
  }>({
    topic: 'sensor_data',
    // No custom select: we want the whole object; default parser handles JSON
  });

  // Default values if no data is available
  const brakeInput = 0//sensorData?.brakeInput ?? 0; // Percentage (0-100)
  const throttleInput = 0//sensorData?.throttleInput ?? 0; // Percentage (0-100)
  const steerVoltage = sensorData?.dynamics?.inverterC ?? 0; // Voltage (0 to 2.5)

  const getStatus = () => {
    if (brakeInput > 5) return 'Braking';
    if (throttleInput > 5) return 'Accelerating';
    console.log(sensorData);
    return 'Idle';
  };

  const status = getStatus();
  const statusColor = {
    Idle: 'text-gray-500',
    Accelerating: 'text-green-500',
    Braking: 'text-red-500',
  }[status];

  const MAX_LEFT_DEGREE = -135; // Placeholder for exact value
  const MAX_RIGHT_DEGREE = 135; // Placeholder for exact value

  const steeringAngle = (steerVoltage - 1.25) * ((MAX_RIGHT_DEGREE - MAX_LEFT_DEGREE) / 2.5); // Map 0-2.5V to MAX_LEFT_DEGREE to MAX_RIGHT_DEGREE

  return (
    <div className="bg-white rounded-lg shadow-md p-4 w-full h-full flex flex-col">
      <h2 className="text-lg font-bold mb-4">Driver Inputs</h2>

      <div className="flex flex-grow">
        {/* Left Column: Pedal Inputs */}
        <div className="flex flex-col items-center justify-around flex-1">
          {/* Pedal Inputs */}
          <div className="flex items-center justify-around w-full">
            {/* Brake Pedal */}
            <div className="flex flex-col items-center">
              <div className="w-12 h-48 bg-gray-200 rounded-lg overflow-hidden relative">
                <div
                  className="absolute bottom-0 w-full bg-red-500"
                  style={{ height: `${brakeInput}%` }}
                ></div>
              </div>
              <span className="mt-2 text-sm font-semibold">{brakeInput}%</span>
              <span className="text-xs text-gray-500">Brake</span>
            </div>

            {/* Throttle Pedal */}
            <div className="flex flex-col items-center">
              <div className="w-12 h-48 bg-gray-200 rounded-lg overflow-hidden relative">
                <div
                  className="absolute bottom-0 w-full bg-green-500"
                  style={{ height: `${throttleInput}%` }}
                ></div>
              </div>
              <span className="mt-2 text-sm font-semibold">{throttleInput}%</span>
              <span className="text-xs text-gray-500">Throttle</span>
            </div>
          </div>
        </div>

        {/* Right Column: Steering Input and Status */}
        <div className="flex flex-col items-center justify-center flex-1">
          {/* Steering Input */}
          <div className="flex flex-col items-center">
            <div style={{ transform: `rotate(${steeringAngle}deg)` }}>
              <SteeringWheelIcon />
            </div>
            <div className="w-full h-4 bg-gray-200 rounded-lg mt-2 relative">
              <div
                className="w-0.5 h-4 bg-black absolute top-0"
                style={{ left: `${((steeringAngle + 135) / 270) * 100}%` }}
              ></div>
            </div>
            <span className="mt-2 text-sm font-semibold">{steeringAngle.toFixed(1)}°</span>
            <span className="text-xs text-gray-500">Steering Angle</span>
          </div>

          {/* Status Indicator */}
          <div className="mt-6 text-center flex flex-col items-center">
            <span className="text-xs text-gray-500">Status</span>
            <span className={`text-lg font-bold ${statusColor}`}>{status}</span>
          </div>
        </div>
      </div>
    </div>
  );
};

export default DriverInputVisualizer;
