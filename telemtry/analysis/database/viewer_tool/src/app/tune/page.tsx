'use client';

import { useState } from 'react';

import AdjustableChart from '@/components/AdjustableChart';

const Section = ({ title, children }: { title: string, children: React.ReactNode }) => (
  <div className="mb-8">
    <h2 className="text-2xl font-bold mb-4 border-b-2 border-gray-300 pb-2">{title}</h2>
    <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
      {children}
    </div>
  </div>
);

const InputField = ({ label }: { label: string }) => (
  <div>
    <label className="block text-gray-700 font-bold mb-1">{label}</label>
    <input type="number" className="shadow appearance-none border rounded w-full py-2 px-3 text-gray-700 leading-tight focus:outline-none focus:shadow-outline" />
  </div>
);

const ChartPlaceholder = ({ title }: { title: string }) => (
  <div className="bg-gray-200 p-4 rounded-lg shadow-inner">
    <h3 className="text-lg font-semibold text-center">{title}</h3>
    <div className="h-64"></div>
  </div>
);

export default function TunePage() {
  return (
    <div className="container mx-auto p-8">
      <h1 className="text-3xl font-bold mb-8">Texas Tune</h1>

      <Section title="APPS">
        <InputField label="Low Pass Filter Time Constant" />
        <InputField label="Implausibility Time" />
        <InputField label="Plausibility Range" />
        <InputField label="1 Voltage Min" />
        <InputField label="1 Voltage Max" />
        <InputField label="2 Voltage Min" />
        <InputField label="2 Voltage Max" />
        <InputField label="Dead Zone Pct" />
      </Section>

      <Section title="BSE">
        <InputField label="Low Pass Filter Time Constant" />
        <InputField label="Implausibility Time" />
        <InputField label="Voltage Min" />
        <InputField label="Voltage Max" />
        <InputField label="Pressure Min" />
        <InputField label="Pressure Max" />
      </Section>

      <Section title="STOMPP">
        <InputField label="Mechanical Brakes Threshold" />
        <InputField label="Apps Cutoff Threshold" />
        <InputField label="Apps Recovery Threshold" />
      </Section>

      <Section title="TORQUE_MAP">
        <AdjustableChart title="Pedal To Torque Request" />
        <AdjustableChart title="Derate Motor Temp" />
        <AdjustableChart title="Derate Inverter Temp" />
        <AdjustableChart title="Derate Battery Temp" />
        <AdjustableChart title="Derate Battery Soc" />
        <InputField label="Map Power Limit" />
        <InputField label="Power Limit Feedback P" />
        <InputField label="Power Limit Feedback Time Constant" />
      </Section>

      <Section title="PRNDL">
        <InputField label="Brake To Start Threshold" />
        <InputField label="Buzzer Duration" />
        <InputField label="Switch Debounce Duration" />
      </Section>

      <Section title="TRACTION_CONTROL">
        <div className="flex items-center">
          <input type="checkbox" id="tcsEnabled" className="mr-2" />
          <label htmlFor="tcsEnabled">Traction Control Enabled</label>
        </div>
        <InputField label="Velocity Low Pass Filter Time Constant" />
        <InputField label="Feedback Low Pass Filter Time Constant" />
      </Section>

      <Section title="STEERING">
        <AdjustableChart title="Wheel To Outer Wheel" />
        <AdjustableChart title="Wheel To Inner Wheel" />
        <InputField label="Pot Max Voltage" />
        <InputField label="Wheel Max Angle" />
      </Section>

    </div>
  );
}