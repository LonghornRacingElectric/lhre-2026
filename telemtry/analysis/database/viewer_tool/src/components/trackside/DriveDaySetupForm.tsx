'use client';

// Reusable drive-day SETUP form (conditions / car setup / shock damping / tires
// / aero), extracted from the standalone Driveday tool so a trackside-live
// session carries the full drive_day record. Controlled: the parent owns the
// `value` (a Partial<DriveDayState>, camelCase) + persists/syncs it. No SSE here
// — trackside-live's own session sync shares it.

import React from 'react';
import { X } from 'lucide-react';
import type { DriveDayState } from '@/lib/types';

export type DriveDaySetup = Partial<DriveDayState>;

// Map the camelCase setup → the snake_case body /api/new-drive-day +
// /api/update-drive-day expect (same mapping the old Driveday form used).
export function driveDaySetupToPayload(s: DriveDaySetup): Record<string, unknown> {
  return {
    track_name: s.trackName, weather: s.weather, wind_speed: s.windSpeed,
    air_temperature: s.airTemperature, relative_humidity: s.relativeHumidity, track_temperature: s.trackTemperature,
    car_weight: s.carWeight, tow_angle: s.towAngle, torque_limit: s.torqueLimit,
    camber_front: s.camberFront, camber_rear: s.camberRear, toe_front: s.toeFront, toe_rear: s.toeRear,
    ride_height_front: s.rideHeightFront, ride_height_rear: s.rideHeightRear,
    fr_lsc: s.frLSC, fr_lsr: s.frLSR, fr_hsc: s.frHSC, fr_hsr: s.frHSR,
    fl_lsc: s.flLSC, fl_lsr: s.flLSR, fl_hsc: s.flHSC, fl_hsr: s.flHSR,
    rr_lsc: s.rrLSC, rr_lsr: s.rrLSR, rr_hsc: s.rrHSC, rr_hsr: s.rrHSR,
    rl_lsc: s.rlLSC, rl_lsr: s.rlLSR, rl_hsc: s.rlHSC, rl_hsr: s.rlHSR,
    frw_pressure: s.frPressure, flw_pressure: s.flPressure, brw_pressure: s.rrPressure, blw_pressure: s.rlPressure,
    frw_hot_pressure: s.frHotPressure, flw_hot_pressure: s.flHotPressure, brw_hot_pressure: s.rrHotPressure, blw_hot_pressure: s.rlHotPressure,
    fr_wear_depth: s.frWearDepth, fl_wear_depth: s.flWearDepth, rr_wear_depth: s.rrWearDepth, rl_wear_depth: s.rlWearDepth,
    fr_durometer: s.frDurometer, fl_durometer: s.flDurometer, rr_durometer: s.rrDurometer, rl_durometer: s.rlDurometer,
    front_wing_on: s.frontWingOn, rear_wing_on: s.rearWingOn,
    front_wing_pitch: s.frontWingPitch, rear_wing_pitch: s.rearWingPitch,
    regen_on: s.regenOn, undertray_on: s.undertrayOn,
    front_corner_spring_rate: s.frontCornerSpringRate, rear_corner_spring_rate: s.rearCornerSpringRate,
    front_arb_setting: s.frontArbSetting, rear_arb_setting: s.rearArbSetting,
  };
}

const inputStyle: React.CSSProperties = {
  width: '100%', font: 'inherit', background: 'var(--surface-alt)', border: '1px solid var(--border)',
  borderRadius: 'var(--radius)', color: 'var(--text)', padding: '5px 8px', boxSizing: 'border-box',
};

export function DriveDaySetupForm({ value, onChange, onClose, disabled }: {
  value: DriveDaySetup;
  onChange: (field: keyof DriveDayState, v: string | boolean | undefined) => void;
  onClose: () => void;
  disabled?: boolean;
}) {
  const txt = (field: keyof DriveDayState, label: string) => (
    <label className="propRow" style={{ display: 'grid', gap: 3 }} key={String(field)}>
      <span className="muted" style={{ fontSize: '0.78rem' }}>{label}</span>
      <input
        value={(value[field] as string) ?? ''} disabled={disabled} style={inputStyle}
        onChange={(e) => onChange(field, e.target.value || undefined)}
      />
    </label>
  );
  const toggle = (field: keyof DriveDayState, label: string) => (
    <label className="checkInline" key={String(field)} style={{ gap: 6 }}>
      <input type="checkbox" checked={!!value[field]} disabled={disabled} onChange={(e) => onChange(field, e.target.checked)} />
      {label}
    </label>
  );
  const arb = (field: keyof DriveDayState, label: string) => (
    <label className="propRow" style={{ display: 'grid', gap: 3 }} key={String(field)}>
      <span className="muted" style={{ fontSize: '0.78rem' }}>{label}</span>
      <select value={(value[field] as string) ?? ''} disabled={disabled} style={inputStyle} onChange={(e) => onChange(field, e.target.value || undefined)}>
        <option value="">—</option><option value="low">Low</option><option value="medium">Medium</option><option value="stiff">Stiff</option>
      </select>
    </label>
  );
  const Section = ({ title, children, cols = 3 }: { title: string; children: React.ReactNode; cols?: number }) => (
    <div style={{ marginBottom: 16 }}>
      <h4 style={{ fontSize: '0.74rem', letterSpacing: 1.5, textTransform: 'uppercase', color: 'var(--muted-text)', borderBottom: '1px solid var(--border)', paddingBottom: 5, margin: '0 0 8px' }}>{title}</h4>
      <div style={{ display: 'grid', gridTemplateColumns: `repeat(${cols}, minmax(0,1fr))`, gap: '8px 12px' }}>{children}</div>
    </div>
  );

  return (
    <div className="modalOverlay" onMouseDown={onClose}>
      <div className="dashEditor" onMouseDown={(e) => e.stopPropagation()} style={{ width: 'min(900px, 96vw)', height: 'min(820px, 92vh)' }}>
        <div className="dashEditorHead">
          <strong>Drive Day Setup</strong>
          <button className="tool iconOnly" onClick={onClose} aria-label="Close"><X size={16} /></button>
        </div>
        <div style={{ overflowY: 'auto', padding: 'var(--s-4)' }}>
          <Section title="Conditions">
            {txt('weather', 'Weather')}{txt('windSpeed', 'Wind (mph)')}{txt('airTemperature', 'Ambient (°F)')}
            {txt('relativeHumidity', 'Humidity (%)')}{txt('trackTemperature', 'Track Temp (°F)')}
          </Section>
          <Section title="Car Setup">
            {txt('carWeight', 'Weight (lbs)')}{txt('torqueLimit', 'Torque Limit (Nm)')}{txt('towAngle', 'Tow (°)')}
            {txt('camberFront', 'Camber F (°)')}{txt('camberRear', 'Camber R (°)')}
            {txt('toeFront', 'Toe F (°)')}{txt('toeRear', 'Toe R (°)')}
            {txt('rideHeightFront', 'Ride Ht F')}{txt('rideHeightRear', 'Ride Ht R')}
          </Section>
          <Section title="Shock Damping (LSC / LSR / HSC / HSR)" cols={4}>
            {txt('frLSC', 'FR LSC')}{txt('frLSR', 'FR LSR')}{txt('frHSC', 'FR HSC')}{txt('frHSR', 'FR HSR')}
            {txt('flLSC', 'FL LSC')}{txt('flLSR', 'FL LSR')}{txt('flHSC', 'FL HSC')}{txt('flHSR', 'FL HSR')}
            {txt('rrLSC', 'RR LSC')}{txt('rrLSR', 'RR LSR')}{txt('rrHSC', 'RR HSC')}{txt('rrHSR', 'RR HSR')}
            {txt('rlLSC', 'RL LSC')}{txt('rlLSR', 'RL LSR')}{txt('rlHSC', 'RL HSC')}{txt('rlHSR', 'RL HSR')}
          </Section>
          <Section title="Tires — Cold psi / Hot psi / Wear / Durometer" cols={4}>
            {txt('frPressure', 'FR cold')}{txt('flPressure', 'FL cold')}{txt('rrPressure', 'RR cold')}{txt('rlPressure', 'RL cold')}
            {txt('frHotPressure', 'FR hot')}{txt('flHotPressure', 'FL hot')}{txt('rrHotPressure', 'RR hot')}{txt('rlHotPressure', 'RL hot')}
            {txt('frWearDepth', 'FR wear')}{txt('flWearDepth', 'FL wear')}{txt('rrWearDepth', 'RR wear')}{txt('rlWearDepth', 'RL wear')}
            {txt('frDurometer', 'FR duro')}{txt('flDurometer', 'FL duro')}{txt('rrDurometer', 'RR duro')}{txt('rlDurometer', 'RL duro')}
          </Section>
          <Section title="Aero & Springs">
            {txt('frontWingPitch', 'Front Wing Pitch')}{txt('rearWingPitch', 'Rear Wing Pitch')}
            {txt('frontCornerSpringRate', 'Front Corner Spring')}{txt('rearCornerSpringRate', 'Rear Corner Spring')}
            {arb('frontArbSetting', 'Front ARB')}{arb('rearArbSetting', 'Rear ARB')}
          </Section>
          <div style={{ display: 'flex', flexWrap: 'wrap', gap: 16, paddingTop: 4 }}>
            {toggle('frontWingOn', 'Front Wing On')}{toggle('rearWingOn', 'Rear Wing On')}
            {toggle('regenOn', 'Regen On')}{toggle('undertrayOn', 'Undertray On')}
          </div>
        </div>
      </div>
    </div>
  );
}

export default DriveDaySetupForm;
