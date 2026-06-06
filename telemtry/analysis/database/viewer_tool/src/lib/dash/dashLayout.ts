// Shared schema for a website-authored dash LAP-CARD layout.
//
// The strategist designs the driver's lap-card screen in the website editor;
// the layout is published once (retained) to lhre/dash/layout and the on-car
// dash (BEVO/dashd React frontend) renders it with the SAME LapCardRenderer.
// This file is the contract — keep it in sync with the dashd copy.
//
// Coordinate space is the dash's fixed panel: 800 x 480, top-left origin, px.

export const LAP_CARD_W = 800;
export const LAP_CARD_H = 480;
export const DASH_LAYOUT_VERSION = 1;

export type WidgetType = 'text' | 'value' | 'bar' | 'gauge';
export type Align = 'left' | 'center' | 'right';

// How a bound numeric value is rendered to text.
export type FormatId =
  | 'raw' | 'int' | 'float1' | 'float2'
  | 'laptime'   // M:SS.ss
  | 'wh' | 'whSigned' | 'kwh'
  | 'kw' | 'pct' | 'temp' | 'volt' | 'amp' | 'mph';

export interface BaseWidget {
  id: string;
  type: WidgetType;
  x: number; y: number; w: number; h: number;
}
export interface TextWidget extends BaseWidget {
  type: 'text';
  text: string;
  fontSize: number;
  color: string;
  align?: Align;
  bold?: boolean;
  letterSpacing?: number;
}
export interface ValueWidget extends BaseWidget {
  type: 'value';
  bind: string;             // dotted path into the data context (see FIELD_CATALOG)
  format: FormatId;
  fontSize: number;
  color: string;
  align?: Align;
  bold?: boolean;
  label?: string;           // small caption above/beside the value
  labelColor?: string;
}
export interface BarWidget extends BaseWidget {
  type: 'bar';
  bind: string;
  min: number; max: number;
  color: string;
  bg?: string;
  bidirectional?: boolean;  // fill from 0 (e.g. budget delta)
  label?: string;
}
export interface GaugeWidget extends BaseWidget {
  type: 'gauge';
  bind: string;
  min: number; max: number;
  label?: string;
  color?: string;           // hex or "gradient"
  mode?: 'standard' | 'bidirectional';
  format?: FormatId;        // center read-out format
}
export type Widget = TextWidget | ValueWidget | BarWidget | GaugeWidget;

export interface LapCardLayout {
  version: number;
  name: string;
  background: string;       // CSS background for the card overlay
  widgets: Widget[];
}

// ---- bindable fields (the "pick a data point" catalog) ---------------------
// Paths resolve against the lap-card data context: { lapCard, pacing, can, mqtt }.
export interface FieldDef {
  bind: string;
  label: string;
  group: string;
  unit?: string;
  defaultFormat: FormatId;
  // sensible defaults for bar/gauge ranges when this field is bound
  min?: number;
  max?: number;
  bidirectional?: boolean;
}

export const FIELD_CATALOG: FieldDef[] = [
  // The just-finished lap (what the card is fundamentally about)
  { bind: 'lapCard.lapNumber', label: 'Lap number', group: 'Lap', defaultFormat: 'int', min: 0, max: 30 },
  { bind: 'lapCard.timeS', label: 'Lap time', group: 'Lap', unit: 's', defaultFormat: 'laptime', min: 0, max: 120 },
  { bind: 'lapCard.energyWh', label: 'Lap energy', group: 'Lap', unit: 'Wh', defaultFormat: 'wh', min: 0, max: 400 },
  { bind: 'mqtt.bestLapTime', label: 'Best lap', group: 'Lap', unit: 's', defaultFormat: 'laptime', min: 0, max: 120 },
  { bind: 'mqtt.lastLapTime', label: 'Last lap', group: 'Lap', unit: 's', defaultFormat: 'laptime', min: 0, max: 120 },
  // Pacing / strategy
  { bind: 'pacing.budgetDeltaWh', label: 'Budget Δ', group: 'Pacing', unit: 'Wh', defaultFormat: 'whSigned', min: -100, max: 100, bidirectional: true },
  { bind: 'pacing.lapEnergyWh', label: 'Energy (this lap)', group: 'Pacing', unit: 'Wh', defaultFormat: 'wh', min: 0, max: 400 },
  { bind: 'mqtt.lapsRemaining', label: 'Laps remaining', group: 'Pacing', defaultFormat: 'int', min: 0, max: 30 },
  { bind: 'mqtt.lapDelta', label: 'Lap delta', group: 'Pacing', unit: 's', defaultFormat: 'float2', min: -5, max: 5, bidirectional: true },
  { bind: 'mqtt.targetPower', label: 'Target power', group: 'Pacing', unit: 'kW', defaultFormat: 'kw', min: 0, max: 80 },
  // Live car
  { bind: 'can.soc', label: 'State of charge', group: 'Battery', unit: '%', defaultFormat: 'pct', min: 0, max: 100 },
  { bind: 'can.temperature', label: 'Battery temp', group: 'Battery', unit: '°C', defaultFormat: 'temp', min: 0, max: 80 },
  { bind: 'can.power', label: 'Power', group: 'Dynamics', unit: 'kW', defaultFormat: 'kw', min: -80, max: 80, bidirectional: true },
  { bind: 'can.speed', label: 'Speed', group: 'Dynamics', unit: 'mph', defaultFormat: 'mph', min: 0, max: 100 },
];

export function fieldDef(bind: string): FieldDef | undefined {
  return FIELD_CATALOG.find((f) => f.bind === bind);
}

// ---- value access + formatting --------------------------------------------
export function getByPath(ctx: unknown, path: string): number | null {
  let cur: unknown = ctx;
  for (const key of path.split('.')) {
    if (cur == null || typeof cur !== 'object') return null;
    cur = (cur as Record<string, unknown>)[key];
  }
  return typeof cur === 'number' && Number.isFinite(cur) ? cur : null;
}

export function formatValue(v: number | null | undefined, fmt: FormatId): string {
  if (v == null || !Number.isFinite(v)) return '--';
  switch (fmt) {
    case 'laptime': {
      const m = Math.floor(v / 60);
      const s = v - m * 60;
      return `${m}:${s.toFixed(2).padStart(5, '0')}`;
    }
    case 'int': return `${Math.round(v)}`;
    case 'float1': return v.toFixed(1);
    case 'float2': return v.toFixed(2);
    case 'wh': return `${Math.round(v)}`;
    case 'whSigned': return `${v >= 0 ? '+' : ''}${Math.round(v)}`;
    case 'kwh': return (v / 1000).toFixed(2);
    case 'kw': return v.toFixed(1);
    case 'pct': return `${Math.round(v)}`;
    case 'temp': return v.toFixed(1);
    case 'volt': return v.toFixed(1);
    case 'amp': return v.toFixed(1);
    case 'mph': return `${Math.round(v)}`;
    default: return `${v}`;
  }
}

// ---- default layout (replicates today's hardcoded lap card) ---------------
export function defaultLapCardLayout(): LapCardLayout {
  return {
    version: DASH_LAYOUT_VERSION,
    name: 'Default lap card',
    background: 'rgba(8,8,10,0.92)',
    widgets: [
      { id: 'lap', type: 'value', bind: 'lapCard.lapNumber', format: 'int', label: 'LAP',
        x: 250, y: 70, w: 300, h: 60, fontSize: 34, color: '#BF5700', align: 'center', bold: true, labelColor: '#BF5700' },
      { id: 'time', type: 'value', bind: 'lapCard.timeS', format: 'laptime',
        x: 100, y: 150, w: 600, h: 160, fontSize: 112, color: '#FFFFFF', align: 'center', bold: true },
      { id: 'energy', type: 'value', bind: 'lapCard.energyWh', format: 'wh', label: 'Wh / LAP',
        x: 250, y: 330, w: 300, h: 70, fontSize: 48, color: '#9aa', align: 'center', bold: true },
    ],
  };
}

// Lenient validation: returns a usable layout or null (caller falls back to default).
export function validateLapCardLayout(raw: unknown): LapCardLayout | null {
  if (!raw || typeof raw !== 'object') return null;
  const o = raw as Partial<LapCardLayout>;
  if (!Array.isArray(o.widgets)) return null;
  const widgets = o.widgets.filter((w): w is Widget =>
    !!w && typeof w === 'object'
    && typeof (w as Widget).id === 'string'
    && ['text', 'value', 'bar', 'gauge'].includes((w as Widget).type)
    && typeof (w as Widget).x === 'number' && typeof (w as Widget).y === 'number');
  return {
    version: typeof o.version === 'number' ? o.version : DASH_LAYOUT_VERSION,
    name: typeof o.name === 'string' ? o.name : 'Lap card',
    background: typeof o.background === 'string' ? o.background : 'rgba(8,8,10,0.92)',
    widgets,
  };
}

// Synthetic data so the editor preview is realistic with the car off.
export function sampleLapCardData() {
  return {
    lapCard: { lapNumber: 7, timeS: 84.36, energyWh: 213 },
    pacing: { lapEnergyWh: 213, budgetDeltaWh: -12, lapElapsedS: 84.36, lapNumber: 7,
      lastLapNumber: 7, lastLapTimeS: 84.36, lastLapEnergyWh: 213 },
    can: { soc: 62, temperature: 41.5, power: 47, speed: 38 },
    mqtt: { lapDelta: -0.42, energyDelta: -12, lapsRemaining: 15, targetPower: 30,
      bestLapTime: 83.1, lastLapTime: 84.36 },
  };
}
