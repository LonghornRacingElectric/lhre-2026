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

export type WidgetType = 'text' | 'value' | 'bar' | 'gauge' | 'delta';
export type Align = 'left' | 'center' | 'right';
// Which sign of a delta counts as "good" (green). Racing default: lower/negative
// is good (faster lap, under energy budget). Flip per-widget when higher is good.
export type GoodSign = 'negative' | 'positive';

export const DELTA_GOOD = '#2ee06a';
export const DELTA_BAD = '#ff4d4f';
export const DELTA_ZERO = '#9aa3ab';

// ---- theme (the lap card follows the dash's dark/light; editor previews both) ----
export type CardTheme = 'dark' | 'light';
export interface ThemePalette { fg: string; muted: string; cardBg: string; track: string; }
export const CARD_THEMES: Record<CardTheme, ThemePalette> = {
  dark:  { fg: '#f2f2f2', muted: '#9b9b9b', cardBg: 'rgba(10,10,12,0.94)', track: 'rgba(255,255,255,0.12)' },
  light: { fg: '#16140f', muted: '#6a635c', cardBg: 'rgba(244,241,237,0.96)', track: 'rgba(0,0,0,0.13)' },
};

// Resolve a widget's color for the active theme:
//   per-mode override (colorLight/colorDark) → explicit base color → "auto" (theme fg).
export function resolveColor(
  w: { color?: string; colorDark?: string; colorLight?: string },
  theme: CardTheme, fallback: string,
): string {
  const override = theme === 'light' ? w.colorLight : w.colorDark;
  if (override) return override;
  if (w.color && w.color !== 'auto') return w.color;
  return fallback;
}

/** Resolve the card background for the theme ('auto'/unset → themed surface). */
export function resolveBackground(bg: string | undefined, theme: CardTheme): string {
  return bg && bg !== 'auto' ? bg : CARD_THEMES[theme].cardBg;
}

/** Color a delta value by sign + which sign is "good". */
export function deltaColor(v: number | null | undefined, goodSign: GoodSign = 'negative',
  good = DELTA_GOOD, bad = DELTA_BAD, zero = DELTA_ZERO): string {
  if (v == null || !Number.isFinite(v) || v === 0) return zero;
  const isGood = goodSign === 'negative' ? v < 0 : v > 0;
  return isGood ? good : bad;
}

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
  color: string;            // hex, or "auto" to follow the theme foreground
  colorDark?: string;       // optional explicit override in dark mode
  colorLight?: string;      // optional explicit override in light mode
  align?: Align;
  bold?: boolean;
  letterSpacing?: number;
}
// A threshold color rule: when the bound value is </> a cutoff, recolor.
// Rules apply in order, last match wins (so order specific→general or vice-versa).
export interface ColorRule { cmp: 'lt' | 'gt'; value: number; color: string; }

export interface ValueWidget extends BaseWidget {
  type: 'value';
  bind: string;             // dotted path into the data context (see FIELD_CATALOG)
  format: FormatId;
  decimals?: number;        // override the format's decimal places (0–6); undefined = format default
  fontSize: number;
  color: string;            // hex, or "auto" to follow the theme foreground
  colorDark?: string;
  colorLight?: string;
  align?: Align;
  bold?: boolean;
  label?: string;           // small caption above/beside the value
  labelColor?: string;
  thresholds?: ColorRule[]; // e.g. [{cmp:'lt',value:20,color:'#ff4d4f'}] for low SoC
  // Map a numeric value to a label (enums / bitfields, e.g. VCU event mode
  // 4 -> "ENDUR"). Keyed by the rounded integer value as a string. When a key
  // matches, its label is shown instead of the numeric format.
  valueMap?: Record<string, string>;
}
export interface BarWidget extends BaseWidget {
  type: 'bar';
  bind: string;
  min: number; max: number;
  color: string;
  bg?: string;
  bidirectional?: boolean;  // fill from 0 (e.g. budget delta)
  signColored?: boolean;    // fill turns green/red by sign (delta bar)
  goodSign?: GoodSign;      // which sign is green when signColored
  label?: string;
}
// A signed delta read-out, auto-colored green/red by sign, with optional ▲/▼.
export interface DeltaWidget extends BaseWidget {
  type: 'delta';
  bind: string;
  format: FormatId;
  decimals?: number;        // override the format's decimal places (0–6)
  fontSize: number;
  align?: Align;
  bold?: boolean;
  label?: string;
  labelColor?: string;
  goodSign?: GoodSign;      // default 'negative' (lower is better)
  goodColor?: string;
  badColor?: string;
  zeroColor?: string;
  showArrow?: boolean;
  showSign?: boolean;       // +/- prefix (default true)
}
export interface GaugeWidget extends BaseWidget {
  type: 'gauge';
  bind: string;
  min: number; max: number;
  label?: string;
  color?: string;           // hex or "gradient"
  colorDark?: string;
  colorLight?: string;
  mode?: 'standard' | 'bidirectional';
  format?: FormatId;        // center read-out format
  decimals?: number;        // override the center read-out's decimal places (0–6)
}
export type Widget = TextWidget | ValueWidget | BarWidget | GaugeWidget | DeltaWidget;

export interface LapCardLayout {
  version: number;
  id?: string;              // stable library id (survives renames); car ignores it
  name: string;
  background: string;       // CSS background for the card overlay
  widgets: Widget[];
}

// ---- bindable fields (the "pick a data point" catalog) ---------------------
// Paths resolve against the dash data context. The lap card binds against
// { lapCard, pacing, can, mqtt }; the park screen against { can, pacing, mqtt }.
// This catalog is intentionally COMPREHENSIVE — every numeric channel dashd
// forwards is exposed so any of them can be bound in the editor ("auto-expose
// all upstream, refine labels/formats later"). `root` (derived from the bind
// path) lets a screen filter to only the contexts it actually receives.
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
  // delta hint: this field is a signed delta, and which sign is "good" (green)
  isDelta?: boolean;
  goodSign?: GoodSign;
  // enum/bitfield fields: value -> label map. When bound, the editor pre-fills a
  // value widget's valueMap with this so it renders labels instead of raw ints.
  enumMap?: Record<string, string>;
}

export const FIELD_CATALOG: FieldDef[] = [
  // The just-finished lap (lap card only — lapCard.* is null on other screens)
  { bind: 'lapCard.lapNumber', label: 'Lap number', group: 'Lap', defaultFormat: 'int', min: 0, max: 30 },
  { bind: 'lapCard.timeS', label: 'Lap time', group: 'Lap', unit: 's', defaultFormat: 'laptime', min: 0, max: 120 },
  { bind: 'lapCard.energyWh', label: 'Lap energy', group: 'Lap', unit: 'Wh', defaultFormat: 'wh', min: 0, max: 400 },
  { bind: 'mqtt.bestLapTime', label: 'Best lap', group: 'Lap', unit: 's', defaultFormat: 'laptime', min: 0, max: 120 },
  { bind: 'mqtt.lastLapTime', label: 'Last lap', group: 'Lap', unit: 's', defaultFormat: 'laptime', min: 0, max: 120 },
  { bind: 'mqtt.currentLapTime', label: 'Current lap time', group: 'Lap', unit: 's', defaultFormat: 'laptime', min: 0, max: 120 },
  { bind: 'mqtt.lapTrigger', label: 'Lap count', group: 'Lap', defaultFormat: 'int', min: 0, max: 30 },
  // Pacing / strategy (on-car authoritative pacing snapshot)
  { bind: 'pacing.lapEnergyWh', label: 'Energy (this lap)', group: 'Pacing', unit: 'Wh', defaultFormat: 'wh', min: 0, max: 400 },
  { bind: 'pacing.lapBudgetWh', label: 'Per-lap budget', group: 'Pacing', unit: 'Wh', defaultFormat: 'wh', min: 0, max: 400 },
  { bind: 'pacing.lapElapsedS', label: 'Lap elapsed', group: 'Pacing', unit: 's', defaultFormat: 'laptime', min: 0, max: 120 },
  { bind: 'pacing.lapNumber', label: 'Lap (in progress)', group: 'Pacing', defaultFormat: 'int', min: 0, max: 30 },
  { bind: 'mqtt.lapsRemaining', label: 'Laps remaining', group: 'Pacing', defaultFormat: 'int', min: 0, max: 30 },
  { bind: 'mqtt.lapsRemainingEnergy', label: 'Laps left (energy)', group: 'Pacing', defaultFormat: 'int', min: 0, max: 30 },
  { bind: 'mqtt.targetPower', label: 'Target power', group: 'Pacing', unit: 'kW', defaultFormat: 'kw', min: 0, max: 80 },
  // Deltas (signed — green/red by sign; lower/negative is "good" by default)
  { bind: 'mqtt.lapDelta', label: 'Lap Δ vs ref', group: 'Deltas', unit: 's', defaultFormat: 'float2', min: -5, max: 5, bidirectional: true, isDelta: true, goodSign: 'negative' },
  { bind: 'pacing.budgetDeltaWh', label: 'Energy budget Δ', group: 'Deltas', unit: 'Wh', defaultFormat: 'whSigned', min: -100, max: 100, bidirectional: true, isDelta: true, goodSign: 'negative' },
  { bind: 'mqtt.energyDelta', label: 'Energy Δ vs target', group: 'Deltas', unit: 'Wh', defaultFormat: 'whSigned', min: -100, max: 100, bidirectional: true, isDelta: true, goodSign: 'negative' },
  { bind: 'mqtt.lapDeltaRate', label: 'Lap Δ rate', group: 'Deltas', unit: 's/s', defaultFormat: 'float2', min: -2, max: 2, bidirectional: true, isDelta: true, goodSign: 'negative' },
  // Energy / charge (VCU is the source of truth for running energy)
  { bind: 'can.soc', label: 'State of energy', group: 'Energy', unit: '%', defaultFormat: 'pct', min: 0, max: 100 },
  { bind: 'can.vcuNetEnergyWh', label: 'VCU net energy', group: 'Energy', unit: 'Wh', defaultFormat: 'wh', min: 0, max: 8000 },
  { bind: 'can.vcuRegenEnergyWh', label: 'VCU regen energy', group: 'Energy', unit: 'Wh', defaultFormat: 'wh', min: 0, max: 2000 },
  { bind: 'can.power', label: 'Pack power', group: 'Energy', unit: 'kW', defaultFormat: 'kw', min: -80, max: 80, bidirectional: true },
  // Cell voltages (pack health)
  { bind: 'can.cellVMax', label: 'Cell V max', group: 'Cells', unit: 'V', defaultFormat: 'volt', min: 2.5, max: 4.3 },
  { bind: 'can.cellVMin', label: 'Cell V min', group: 'Cells', unit: 'V', defaultFormat: 'volt', min: 2.5, max: 4.3 },
  { bind: 'can.cellVSpread', label: 'Cell V spread', group: 'Cells', unit: 'V', defaultFormat: 'float2', min: 0, max: 0.5 },
  // Cell + pack temps
  { bind: 'can.cellTempMax', label: 'Cell T max', group: 'Temps', unit: '°C', defaultFormat: 'temp', min: 0, max: 80 },
  { bind: 'can.cellTempAvg', label: 'Cell T avg', group: 'Temps', unit: '°C', defaultFormat: 'temp', min: 0, max: 80 },
  { bind: 'can.cellTempMin', label: 'Cell T min', group: 'Temps', unit: '°C', defaultFormat: 'temp', min: 0, max: 80 },
  { bind: 'can.temperature', label: 'Battery temp', group: 'Temps', unit: '°C', defaultFormat: 'temp', min: 0, max: 80 },
  // Powertrain temps
  { bind: 'can.motorTemp', label: 'Motor temp', group: 'Temps', unit: '°C', defaultFormat: 'temp', min: 0, max: 120 },
  { bind: 'can.inverterTemp', label: 'Inverter temp', group: 'Temps', unit: '°C', defaultFormat: 'temp', min: 0, max: 100 },
  { bind: 'can.coolantTemp', label: 'Coolant temp', group: 'Temps', unit: '°C', defaultFormat: 'temp', min: 0, max: 80 },
  // Driver inputs
  { bind: 'can.apps', label: 'APPS (accel)', group: 'Driver', unit: '%', defaultFormat: 'pct', min: 0, max: 100 },
  { bind: 'can.bpps', label: 'BPPS (brake)', group: 'Driver', unit: '%', defaultFormat: 'pct', min: 0, max: 100 },
  { bind: 'can.brakeBias', label: 'Brake bias (front)', group: 'Driver', unit: '%', defaultFormat: 'pct', min: 0, max: 100 },
  { bind: 'can.brakePressureFront', label: 'Brake pressure F', group: 'Driver', unit: 'psi', defaultFormat: 'int', min: 0, max: 1000 },
  { bind: 'can.brakePressureRear', label: 'Brake pressure R', group: 'Driver', unit: 'psi', defaultFormat: 'int', min: 0, max: 1000 },
  // Rails
  { bind: 'can.hvVoltage', label: 'HV voltage', group: 'Rails', unit: 'V', defaultFormat: 'volt', min: 0, max: 600 },
  { bind: 'can.hvCurrent', label: 'HV current', group: 'Rails', unit: 'A', defaultFormat: 'amp', min: -400, max: 400, bidirectional: true },
  { bind: 'can.lvVoltage', label: 'LV voltage', group: 'Rails', unit: 'V', defaultFormat: 'volt', min: 0, max: 30 },
  { bind: 'can.lvCurrent', label: 'LV current', group: 'Rails', unit: 'A', defaultFormat: 'amp', min: 0, max: 30 },
  // Dynamics + wheels
  { bind: 'can.speed', label: 'Speed', group: 'Dynamics', unit: 'mph', defaultFormat: 'mph', min: 0, max: 100 },
  { bind: 'can.eventMode', label: 'VCU event mode', group: 'Dynamics', defaultFormat: 'int', min: 0, max: 4,
    enumMap: { '0': '—', '1': 'ACCEL', '2': 'SKID', '3': 'AUTOX', '4': 'ENDUR' } },
  { bind: 'can.wheelSpeedFL', label: 'Wheel FL', group: 'Wheels', defaultFormat: 'float1', min: 0, max: 100 },
  { bind: 'can.wheelSpeedFR', label: 'Wheel FR', group: 'Wheels', defaultFormat: 'float1', min: 0, max: 100 },
  { bind: 'can.wheelSpeedRL', label: 'Wheel RL', group: 'Wheels', defaultFormat: 'float1', min: 0, max: 100 },
  { bind: 'can.wheelSpeedRR', label: 'Wheel RR', group: 'Wheels', defaultFormat: 'float1', min: 0, max: 100 },
];

export function fieldDef(bind: string): FieldDef | undefined {
  return FIELD_CATALOG.find((f) => f.bind === bind);
}

// The top-level context object a bind path reads from (e.g. 'can.cellVMax' -> 'can').
export function bindRoot(bind: string): string {
  return bind.split('.')[0];
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

// Pick a value's color: base, overridden by any matching threshold rule (last wins).
export function valueColor(v: number | null | undefined, base: string, rules?: ColorRule[]): string {
  if (v == null || !Number.isFinite(v) || !rules?.length) return base;
  let c = base;
  for (const r of rules) {
    if ((r.cmp === 'lt' && v < r.value) || (r.cmp === 'gt' && v > r.value)) c = r.color;
  }
  return c;
}

export function formatValue(v: number | null | undefined, fmt: FormatId, decimals?: number): string {
  if (v == null || !Number.isFinite(v)) return '--';
  // Optional per-widget precision override. Applies to the numeric formats —
  // keeps kwh's /1000 scaling and whSigned's +/- sign; lap-time ignores it.
  if (decimals != null && Number.isFinite(decimals) && fmt !== 'laptime') {
    const d = Math.max(0, Math.min(6, Math.floor(decimals)));
    if (fmt === 'kwh') return (v / 1000).toFixed(d);
    if (fmt === 'whSigned') return `${v >= 0 ? '+' : ''}${v.toFixed(d)}`;
    return v.toFixed(d);
  }
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

// Value widgets with a valueMap (enums/bitfields) show the mapped label; anything
// without a matching key falls back to the numeric format.
export function applyValueMap(v: number | null | undefined, fmt: FormatId, valueMap?: Record<string, string>, decimals?: number): string {
  if (v != null && Number.isFinite(v) && valueMap) {
    const label = valueMap[String(Math.round(v))];
    if (label != null) return label;
  }
  return formatValue(v, fmt, decimals);
}

// ---- default layout (replicates today's hardcoded lap card) ---------------
export function defaultLapCardLayout(): LapCardLayout {
  return {
    version: DASH_LAYOUT_VERSION,
    name: 'Default lap card',
    background: 'auto',
    widgets: [
      { id: 'lap', type: 'value', bind: 'lapCard.lapNumber', format: 'int', label: 'LAP',
        x: 250, y: 70, w: 300, h: 60, fontSize: 34, color: '#d97757', align: 'center', bold: true, labelColor: '#d97757' },
      { id: 'time', type: 'value', bind: 'lapCard.timeS', format: 'laptime',
        x: 100, y: 150, w: 600, h: 160, fontSize: 112, color: 'auto', align: 'center', bold: true },
      { id: 'energy', type: 'value', bind: 'lapCard.energyWh', format: 'wh', label: 'Wh / LAP',
        x: 250, y: 330, w: 300, h: 70, fontSize: 48, color: 'auto', align: 'center', bold: true },
    ],
  };
}

// ---- screens ---------------------------------------------------------------
// Each editable dash screen authors independently: its own retained MQTT topic
// (lhre/dash/<topic>), library namespace, default layout, and the set of data
// contexts available to bind. Add a screen here + a render site that reads the
// matching DashMessage field, and the unified editor picks it up automatically.
export interface ScreenDef {
  id: string;
  name: string;
  topic: string;            // lhre/dash/<topic> (retained)
  libraryKey: string;       // localStorage key + server library namespace
  contextRoots: string[];   // top-level ctx objects the screen receives
  build: () => LapCardLayout;
}

// Default starter for the Park / pit-debug screen — SoE, cell-voltage health,
// running VCU energy, key temps, brake bias. All can.* (no lapCard.* — that
// only exists on the lap card).
export function defaultParkLayout(): LapCardLayout {
  return {
    version: DASH_LAYOUT_VERSION,
    name: 'Default park screen',
    background: 'auto',
    widgets: [
      { id: 'soe', type: 'value', bind: 'can.soc', format: 'pct', label: 'SoE %',
        x: 30, y: 24, w: 320, h: 150, fontSize: 110, color: 'auto', align: 'center', bold: true },
      { id: 'vmax', type: 'value', bind: 'can.cellVMax', format: 'volt', label: 'CELL V MAX',
        x: 380, y: 30, w: 180, h: 96, fontSize: 46, color: 'auto', align: 'center', bold: true },
      { id: 'vmin', type: 'value', bind: 'can.cellVMin', format: 'volt', label: 'CELL V MIN',
        x: 580, y: 30, w: 180, h: 96, fontSize: 46, color: 'auto', align: 'center', bold: true },
      { id: 'vspread', type: 'value', bind: 'can.cellVSpread', format: 'float2', label: 'SPREAD V',
        x: 380, y: 140, w: 180, h: 96, fontSize: 46, color: 'auto', align: 'center', bold: true,
        thresholds: [{ cmp: 'gt', value: 0.05, color: '#FFD700' }, { cmp: 'gt', value: 0.1, color: '#FF3333' }] },
      { id: 'net', type: 'value', bind: 'can.vcuNetEnergyWh', format: 'wh', label: 'NET Wh',
        x: 580, y: 140, w: 180, h: 96, fontSize: 46, color: 'auto', align: 'center', bold: true },
      { id: 'tmax', type: 'value', bind: 'can.cellTempMax', format: 'temp', label: 'CELL T MAX',
        x: 30, y: 260, w: 180, h: 90, fontSize: 42, color: 'auto', align: 'center', bold: true,
        thresholds: [{ cmp: 'gt', value: 50, color: '#FFD700' }, { cmp: 'gt', value: 60, color: '#FF3333' }] },
      { id: 'motor', type: 'value', bind: 'can.motorTemp', format: 'temp', label: 'MOTOR °C',
        x: 230, y: 260, w: 180, h: 90, fontSize: 42, color: 'auto', align: 'center', bold: true },
      { id: 'inv', type: 'value', bind: 'can.inverterTemp', format: 'temp', label: 'INV °C',
        x: 430, y: 260, w: 160, h: 90, fontSize: 42, color: 'auto', align: 'center', bold: true },
      { id: 'hv', type: 'value', bind: 'can.hvVoltage', format: 'volt', label: 'HV V',
        x: 610, y: 260, w: 150, h: 90, fontSize: 42, color: 'auto', align: 'center', bold: true },
      { id: 'bias', type: 'value', bind: 'can.brakeBias', format: 'pct', label: 'BIAS F %',
        x: 30, y: 372, w: 180, h: 84, fontSize: 40, color: 'auto', align: 'center', bold: true },
      { id: 'pwr', type: 'value', bind: 'can.power', format: 'kw', label: 'PACK kW',
        x: 230, y: 372, w: 180, h: 84, fontSize: 40, color: 'auto', align: 'center', bold: true },
      { id: 'regen', type: 'value', bind: 'can.vcuRegenEnergyWh', format: 'wh', label: 'REGEN Wh',
        x: 430, y: 372, w: 330, h: 84, fontSize: 40, color: 'auto', align: 'center', bold: true },
    ],
  };
}

export const DASH_SCREENS: ScreenDef[] = [
  { id: 'lapCard', name: 'Lap Card', topic: 'layout', libraryKey: 'dash-lap-layouts',
    contextRoots: ['lapCard', 'pacing', 'can', 'mqtt'], build: defaultLapCardLayout },
  { id: 'park', name: 'Park / Pit', topic: 'parkLayout', libraryKey: 'dash-park-layouts',
    contextRoots: ['can', 'pacing', 'mqtt'], build: defaultParkLayout },
];

export function screenDef(id: string): ScreenDef {
  return DASH_SCREENS.find((s) => s.id === id) ?? DASH_SCREENS[0];
}

// Catalog fields a screen can bind — only those whose context it actually
// receives (so the park picker doesn't offer lapCard.* which is always null).
export function fieldsForScreen(screenId: string): FieldDef[] {
  const roots = screenDef(screenId).contextRoots;
  return FIELD_CATALOG.filter((f) => roots.includes(bindRoot(f.bind)));
}

// Starter presets the strategist can load and then tweak.
export const LAYOUT_TEMPLATES: { id: string; name: string; build: () => LapCardLayout }[] = [
  { id: 'default', name: 'Default lap card', build: defaultLapCardLayout },
  {
    id: 'delta', name: 'Delta board',
    build: () => ({
      version: DASH_LAYOUT_VERSION, name: 'Delta board', background: 'auto',
      widgets: [
        { id: 'time', type: 'value', bind: 'lapCard.timeS', format: 'laptime', x: 100, y: 40, w: 600, h: 130, fontSize: 96, color: 'auto', align: 'center', bold: true },
        { id: 'lapd', type: 'delta', bind: 'mqtt.lapDelta', format: 'float2', x: 60, y: 220, w: 320, h: 130, fontSize: 80, align: 'center', bold: true, label: 'LAP Δ', goodSign: 'negative', showArrow: true, showSign: true },
        { id: 'budd', type: 'delta', bind: 'pacing.budgetDeltaWh', format: 'whSigned', x: 420, y: 220, w: 320, h: 130, fontSize: 80, align: 'center', bold: true, label: 'BUDGET Δ Wh', goodSign: 'negative', showArrow: true, showSign: true },
        { id: 'lapn', type: 'value', bind: 'lapCard.lapNumber', format: 'int', label: 'LAP', x: 300, y: 380, w: 200, h: 60, fontSize: 40, color: '#d97757', align: 'center', bold: true },
      ],
    }),
  },
  {
    id: 'energy', name: 'Energy strategy',
    build: () => ({
      version: DASH_LAYOUT_VERSION, name: 'Energy strategy', background: 'auto',
      widgets: [
        { id: 'soc', type: 'gauge', bind: 'can.soc', min: 0, max: 100, label: 'SOC', color: 'gradient', mode: 'standard', format: 'pct', x: 60, y: 130, w: 220, h: 220 },
        { id: 'lapnrg', type: 'value', bind: 'lapCard.energyWh', format: 'wh', label: 'Wh / LAP', x: 320, y: 90, w: 420, h: 120, fontSize: 84, color: 'auto', align: 'center', bold: true },
        { id: 'budbar', type: 'bar', bind: 'pacing.budgetDeltaWh', min: -100, max: 100, color: '#d97757', bidirectional: true, signColored: true, goodSign: 'negative', label: 'BUDGET Δ', x: 320, y: 240, w: 420, h: 56 },
        { id: 'laps', type: 'value', bind: 'mqtt.lapsRemaining', format: 'int', label: 'LAPS LEFT', x: 320, y: 330, w: 420, h: 100, fontSize: 64, color: 'auto', align: 'center', bold: true },
      ],
    }),
  },
  {
    id: 'laptime', name: 'Lap-time focus',
    build: () => ({
      version: DASH_LAYOUT_VERSION, name: 'Lap-time focus', background: 'auto',
      widgets: [
        { id: 'lapn', type: 'value', bind: 'lapCard.lapNumber', format: 'int', label: 'LAP', x: 250, y: 50, w: 300, h: 60, fontSize: 38, color: '#d97757', align: 'center', bold: true },
        { id: 'time', type: 'value', bind: 'lapCard.timeS', format: 'laptime', x: 60, y: 140, w: 680, h: 180, fontSize: 128, color: 'auto', align: 'center', bold: true },
        { id: 'best', type: 'value', bind: 'mqtt.bestLapTime', format: 'laptime', label: 'BEST', x: 250, y: 350, w: 300, h: 80, fontSize: 44, color: 'auto', align: 'center', bold: true },
      ],
    }),
  },
];

// Lenient validation: returns a usable layout or null (caller falls back to default).
export function validateLapCardLayout(raw: unknown): LapCardLayout | null {
  if (!raw || typeof raw !== 'object') return null;
  const o = raw as Partial<LapCardLayout>;
  if (!Array.isArray(o.widgets)) return null;
  const widgets = o.widgets.filter((w): w is Widget =>
    !!w && typeof w === 'object'
    && typeof (w as Widget).id === 'string'
    && ['text', 'value', 'bar', 'gauge', 'delta'].includes((w as Widget).type)
    && typeof (w as Widget).x === 'number' && typeof (w as Widget).y === 'number');
  return {
    version: typeof o.version === 'number' ? o.version : DASH_LAYOUT_VERSION,
    id: typeof o.id === 'string' ? o.id : undefined,
    name: typeof o.name === 'string' ? o.name : 'Lap card',
    background: typeof o.background === 'string' ? o.background : 'auto',
    widgets,
  };
}

// Synthetic data so the editor preview is realistic with the car off. Covers
// every catalog field across all contexts so any bound widget renders a value
// (both the lap card and the park screen preview from this).
export function sampleLapCardData() {
  return {
    lapCard: { lapNumber: 7, timeS: 84.36, energyWh: 213 },
    pacing: { lapEnergyWh: 213, budgetDeltaWh: -12, lapBudgetWh: 225, lapElapsedS: 84.36,
      lapNumber: 7, lastLapNumber: 7, lastLapTimeS: 84.36, lastLapEnergyWh: 213 },
    can: {
      soc: 62, temperature: 41.5, power: 47, speed: 38, eventMode: 4,
      vcuNetEnergyWh: 4120, vcuRegenEnergyWh: 615,
      cellVMax: 4.02, cellVMin: 3.91, cellVSpread: 0.11,
      cellTempMax: 46.2, cellTempAvg: 42.8, cellTempMin: 38.4,
      motorTemp: 64, inverterTemp: 52, coolantTemp: 38,
      apps: 0, bpps: 0, brakeBias: 54, brakePressureFront: 0, brakePressureRear: 0,
      hvVoltage: 449.3, hvCurrent: 10, lvVoltage: 25, lvCurrent: 9.3,
      wheelSpeedFL: 38, wheelSpeedFR: 38, wheelSpeedRL: 38, wheelSpeedRR: 38,
    },
    mqtt: { lapDelta: -0.42, energyDelta: -12, lapsRemaining: 15, lapsRemainingEnergy: 14,
      targetPower: 30, bestLapTime: 83.1, lastLapTime: 84.36, currentLapTime: 84.36,
      lapDeltaRate: 0.08, lapTrigger: 7 },
  };
}
