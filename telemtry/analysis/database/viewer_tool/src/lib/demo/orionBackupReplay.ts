import { createReadStream, promises as fs } from "fs";
import path from "path";
import readline from "readline";

export type BackupDashboardData = {
  packetId?: number | null;
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

export type BackupLiveBannerData = {
  battery?: number | null;
  odometer?: number | null;
};

export type BackupEnergyBudgetData = {
  powerKw?: number | null;
  timeSinceOnS?: number | null;
  batteryPct?: number | null;
};

export type BackupMapData = {
  dynamics?: { gps?: number[] | null };
};

export type BackupSensorData = {
  dynamics?: Record<string, unknown>;
  pack?: Record<string, unknown>;
  thermal?: Record<string, unknown>;
};

export type OrionBackupFrame = {
  timestampMs: number;
  dashboard: BackupDashboardData;
  liveBanner: BackupLiveBannerData;
  energy: BackupEnergyBudgetData;
  map: BackupMapData;
  sensor: BackupSensorData;
};

export type OrionBackupReplay = {
  fileName: string;
  sourcePath: string;
  sampleMs: number;
  trimStartMs: number;
  trimEndMs: number;
  durationMs: number;
  frameCount: number;
  cellTemps: number[] | null;
  frames: OrionBackupFrame[];
};

const SAMPLE_MS = 100;
const ACTIVE_GAP_MS = 3000;
const TRIM_PADDING_MS = 3000;
const MOTOR_ACTIVE_RPM = 500;
const THROTTLE_ACTIVE = 0.08;
const GPS_ACTIVE_MPS = 1.0;
const MPS_TO_MPH = 2.2369362920544;
const METERS_TO_MILES = 0.0006213711922373339;
const DEFAULT_BACKUP_BASENAME = "orion_log_better";
const APPS_VOLTAGE_MIN = 1.2;
const APPS_VOLTAGE_MAX = 3.8;
const BRAKE_VOLTAGE_MIN = 0.3;
const BRAKE_VOLTAGE_MAX = 1.15;

type IndexMap = Record<string, number>;
type Segment = {
  startMs: number;
  endMs: number;
  variationScore: number;
};

let replayCache: Promise<OrionBackupReplay> | null = null;
let replayCacheKey: string | null = null;

function splitCsvLine(line: string): string[] {
  const out: string[] = [];
  let cur = "";
  let inQuotes = false;

  for (let i = 0; i < line.length; i += 1) {
    const ch = line[i];
    if (ch === '"') {
      if (inQuotes && line[i + 1] === '"') {
        cur += '"';
        i += 1;
      } else {
        inQuotes = !inQuotes;
      }
      continue;
    }

    if (ch === "," && !inQuotes) {
      out.push(cur);
      cur = "";
      continue;
    }

    cur += ch;
  }

  out.push(cur);
  return out;
}

function toFiniteNumber(value: string | undefined): number | undefined {
  if (value === undefined || value === "") return undefined;
  const n = Number(value);
  return Number.isFinite(n) ? n : undefined;
}

function parseNumberArray(value: string | undefined): number[] | undefined {
  if (!value || value === "[]") return undefined;
  try {
    const parsed = JSON.parse(value);
    if (!Array.isArray(parsed)) return undefined;
    const values = parsed
      .map((entry) => (typeof entry === "number" ? entry : Number(entry)))
      .filter((entry): entry is number => Number.isFinite(entry));
    return values.length ? values : undefined;
  } catch {
    return undefined;
  }
}

function parseGps(value: string | undefined): [number, number] | undefined {
  const values = parseNumberArray(value);
  if (!values || values.length < 2) return undefined;
  return [values[0], values[1]];
}

function clamp(value: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, value));
}

function normalizePct(value: number | undefined): number | undefined {
  if (value === undefined) return undefined;
  if (!Number.isFinite(value)) return undefined;
  if (value <= 1.2) return clamp(value * 100, 0, 100);
  return clamp(value, 0, 100);
}

function appsVoltageToPct(apps1V: number | undefined, apps2V: number | undefined): number | undefined {
  const values = [apps1V, apps2V].filter((entry): entry is number => typeof entry === "number");
  if (!values.length) return undefined;
  const avgV = values.reduce((sum, entry) => sum + entry, 0) / values.length;
  const pct = ((avgV - APPS_VOLTAGE_MIN) / (APPS_VOLTAGE_MAX - APPS_VOLTAGE_MIN)) * 100;
  return clamp(pct, 0, 100);
}

function brakeVoltageToPct(bse1V: number | undefined, bse2V: number | undefined): number | undefined {
  const values = [bse1V, bse2V].filter((entry): entry is number => typeof entry === "number");
  if (!values.length) return undefined;
  const pedalV = Math.max(...values);
  const pct = ((pedalV - BRAKE_VOLTAGE_MIN) / (BRAKE_VOLTAGE_MAX - BRAKE_VOLTAGE_MIN)) * 100;
  return clamp(pct, 0, 100);
}

function brakePressureToPctInverted(pressure: number | undefined): number | undefined {
  if (pressure === undefined || !Number.isFinite(pressure)) return undefined;
  // This Orion log's pressure channel behaves inverse (higher baseline when not braking).
  const normalized = clamp(pressure / 300, 0, 1);
  return (1 - normalized) * 100;
}

function firstFinite(...values: Array<number | undefined>): number | undefined {
  for (const value of values) {
    if (typeof value === "number" && Number.isFinite(value)) {
      return value;
    }
  }
  return undefined;
}

function firstNonZero(...values: Array<number | undefined>): number | undefined {
  for (const value of values) {
    if (typeof value === "number" && Number.isFinite(value) && Math.abs(value) > 1e-6) {
      return value;
    }
  }
  return undefined;
}

function estimateSocFromVoltage(hvPackV: number | undefined): number | undefined {
  if (hvPackV === undefined || !Number.isFinite(hvPackV) || hvPackV <= 0) return undefined;
  // Rough Orion backup estimate window from observed DC bus behavior.
  return clamp(((hvPackV - 320) / (540 - 320)) * 100, 0, 100);
}

function haversineMeters(a: [number, number], b: [number, number]): number {
  const [lat1, lon1] = a;
  const [lat2, lon2] = b;
  const toRad = Math.PI / 180;
  const dLat = (lat2 - lat1) * toRad;
  const dLon = (lon2 - lon1) * toRad;
  const lat1Rad = lat1 * toRad;
  const lat2Rad = lat2 * toRad;
  const x =
    Math.sin(dLat / 2) * Math.sin(dLat / 2) +
    Math.sin(dLon / 2) * Math.sin(dLon / 2) * Math.cos(lat1Rad) * Math.cos(lat2Rad);
  return 6371000 * (2 * Math.atan2(Math.sqrt(x), Math.sqrt(1 - x)));
}

function field(values: string[], indexMap: IndexMap, name: string): string | undefined {
  const idx = indexMap[name];
  if (idx === undefined || idx < 0 || idx >= values.length) return undefined;
  return values[idx];
}

function closeSegment(
  segments: Segment[],
  startMs: number | null,
  endMs: number | null,
  variationScore: number,
) {
  if (startMs === null || endMs === null || endMs < startMs) return;
  segments.push({ startMs, endMs, variationScore });
}

async function resolveOrionLogFile(fileName?: string): Promise<{ fileName: string; filePath: string }> {
  const logsDir = path.resolve(process.cwd(), "../../../logs/orion");
  const entries = await fs.readdir(logsDir);
  const csvFiles = entries.filter((entry) => entry.toLowerCase().endsWith(".csv")).sort();
  if (!csvFiles.length) {
    throw new Error(`No CSV files found in ${logsDir}`);
  }

  if (fileName) {
    const normalizedRequested = fileName.toLowerCase().endsWith(".csv")
      ? fileName
      : `${fileName}.csv`;
    const requested = csvFiles.find((entry) => entry === normalizedRequested);
    if (!requested) {
      throw new Error(`CSV file '${fileName}' not found in ${logsDir}`);
    }
    return { fileName: requested, filePath: path.join(logsDir, requested) };
  }

  const preferred = csvFiles.find(
    (entry) => entry.toLowerCase() === `${DEFAULT_BACKUP_BASENAME}.csv`,
  );
  const selected = preferred ?? csvFiles[csvFiles.length - 1];
  return { fileName: selected, filePath: path.join(logsDir, selected) };
}

async function detectTrimWindow(filePath: string): Promise<{ totalStartMs: number; totalEndMs: number; trimStartMs: number; trimEndMs: number }> {
  const file = createReadStream(filePath);
  const rl = readline.createInterface({ input: file, crlfDelay: Infinity });

  let headerParsed = false;
  const indexMap: IndexMap = {};

  let totalStartMs: number | null = null;
  let totalEndMs: number | null = null;

  let prevGps: [number, number] | undefined;
  let prevGpsTimeMs: number | undefined;

  let currentSegStart: number | null = null;
  let currentSegEnd: number | null = null;
  let currentSegScore = 0;
  let currentSegLastGpsSpeedMps: number | undefined;
  const segments: Segment[] = [];

  for await (const line of rl) {
    if (!headerParsed) {
      const headers = splitCsvLine(line);
      headers.forEach((name, idx) => {
        indexMap[name] = idx;
      });
      headerParsed = true;
      continue;
    }

    if (!line) continue;
    const values = splitCsvLine(line);
    const timeMs = toFiniteNumber(field(values, indexMap, "packet.time"));
    if (timeMs === undefined) continue;

    if (totalStartMs === null) totalStartMs = timeMs;
    totalEndMs = timeMs;

    const gps = parseGps(field(values, indexMap, "dynamics.gps"));
    const motorSpeed = Math.abs(toFiniteNumber(field(values, indexMap, "controls.motor_speed")) ?? 0);
    const throttle = toFiniteNumber(field(values, indexMap, "dynamics.accel_pedal_travel")) ?? 0;
    const brakePressure = toFiniteNumber(field(values, indexMap, "controls.brake_pressure_f")) ?? 0;

    let gpsSpeedMps = 0;
    if (gps && prevGps && prevGpsTimeMs !== undefined && timeMs > prevGpsTimeMs) {
      const distance = haversineMeters(prevGps, gps);
      const dtSeconds = (timeMs - prevGpsTimeMs) / 1000;
      if (dtSeconds > 0) {
        gpsSpeedMps = distance / dtSeconds;
      }
    }
    if (gps) {
      prevGps = gps;
      prevGpsTimeMs = timeMs;
    }

    const isActive =
      motorSpeed >= MOTOR_ACTIVE_RPM ||
      throttle >= THROTTLE_ACTIVE ||
      brakePressure >= 12 ||
      gpsSpeedMps >= GPS_ACTIVE_MPS;

    if (isActive) {
      if (currentSegStart === null) {
        currentSegStart = timeMs;
        currentSegScore = 0;
        currentSegLastGpsSpeedMps = undefined;
      }
      currentSegEnd = timeMs;
      if (gps && prevGps && prevGpsTimeMs !== undefined && timeMs > prevGpsTimeMs) {
        const distance = haversineMeters(prevGps, gps);
        if (Number.isFinite(distance) && distance >= 0) {
          currentSegScore += Math.min(distance, 60);
        }
        if (currentSegLastGpsSpeedMps !== undefined) {
          currentSegScore += Math.min(Math.abs(gpsSpeedMps - currentSegLastGpsSpeedMps), 20) * 4;
        }
        currentSegLastGpsSpeedMps = gpsSpeedMps;
      }
      continue;
    }

    if (currentSegStart !== null && currentSegEnd !== null && timeMs - currentSegEnd > ACTIVE_GAP_MS) {
      closeSegment(segments, currentSegStart, currentSegEnd, currentSegScore);
      currentSegStart = null;
      currentSegEnd = null;
      currentSegScore = 0;
      currentSegLastGpsSpeedMps = undefined;
    }
  }

  closeSegment(segments, currentSegStart, currentSegEnd, currentSegScore);

  if (totalStartMs === null || totalEndMs === null) {
    throw new Error(`CSV '${filePath}' has no usable rows`);
  }

  const bestSegment =
    segments.length > 0
      ? segments.reduce((best, seg) => {
          const bestDurationMs = Math.max(1000, best.endMs - best.startMs);
          const segDurationMs = Math.max(1000, seg.endMs - seg.startMs);
          const bestDurationWeight = Math.min(1, bestDurationMs / 60000);
          const segDurationWeight = Math.min(1, segDurationMs / 60000);
          const bestRank = (best.variationScore / bestDurationMs) * bestDurationWeight;
          const segRank = (seg.variationScore / segDurationMs) * segDurationWeight;
          if (segRank > bestRank) return seg;
          if (segRank === bestRank && seg.variationScore > best.variationScore) return seg;
          return best;
        })
      : { startMs: totalStartMs, endMs: totalEndMs, variationScore: 0 };

  const trimStartMs = Math.max(totalStartMs, bestSegment.startMs - TRIM_PADDING_MS);
  const trimEndMs = Math.min(totalEndMs, bestSegment.endMs + TRIM_PADDING_MS);

  return { totalStartMs, totalEndMs, trimStartMs, trimEndMs };
}

async function buildReplay(fileName?: string): Promise<OrionBackupReplay> {
  const { fileName: resolvedFileName, filePath } = await resolveOrionLogFile(fileName);
  const { trimStartMs, trimEndMs } = await detectTrimWindow(filePath);

  const file = createReadStream(filePath);
  const rl = readline.createInterface({ input: file, crlfDelay: Infinity });

  let headerParsed = false;
  const indexMap: IndexMap = {};
  const frames: OrionBackupFrame[] = [];
  let cachedCellTemps: number[] | null = null;

  let nextSampleMs = trimStartMs;
  let prevGps: [number, number] | undefined;
  let prevGpsTimeMs: number | undefined;
  let lastGps: [number, number] | undefined;
  let speedMph = 0;
  let odometerMiles = 0;
  let lastHvPackV: number | undefined;
  let lastHvCurrent: number | undefined;
  let lastHvSoc: number | undefined;
  let lastLvV: number | undefined;
  let lastInverterTemp: number | undefined;
  let lastMotorTemp: number | undefined;
  let lastAmbientTemp: number | undefined;

  for await (const line of rl) {
    if (!headerParsed) {
      const headers = splitCsvLine(line);
      headers.forEach((name, idx) => {
        indexMap[name] = idx;
      });
      headerParsed = true;
      continue;
    }

    if (!line) continue;
    const values = splitCsvLine(line);
    const timeMs = toFiniteNumber(field(values, indexMap, "packet.time"));
    if (timeMs === undefined) continue;
    if (timeMs < trimStartMs) continue;
    if (timeMs > trimEndMs) break;

    const packetId = toFiniteNumber(field(values, indexMap, "packet.packet_id"));
    const gps = parseGps(field(values, indexMap, "dynamics.gps"));
    if (gps) {
      if (prevGps && prevGpsTimeMs !== undefined && timeMs > prevGpsTimeMs) {
        const distance = haversineMeters(prevGps, gps);
        const dtSec = (timeMs - prevGpsTimeMs) / 1000;
        if (dtSec > 0) {
          const nextSpeedMps = distance / dtSec;
          if (Number.isFinite(nextSpeedMps) && nextSpeedMps >= 0 && nextSpeedMps < 90) {
            speedMph = nextSpeedMps * MPS_TO_MPH;
          }
        }
        if (Number.isFinite(distance) && distance >= 0 && distance < 100) {
          odometerMiles += distance * METERS_TO_MILES;
        }
      }
      prevGps = gps;
      prevGpsTimeMs = timeMs;
      lastGps = gps;
    }

    if (timeMs < nextSampleMs) continue;

    const accelPedal = toFiniteNumber(field(values, indexMap, "dynamics.accel_pedal_travel"));
    const apps1Travel = toFiniteNumber(field(values, indexMap, "controls.apps1_travel"));
    const apps2Travel = toFiniteNumber(field(values, indexMap, "controls.apps2_travel"));
    const apps1V = toFiniteNumber(field(values, indexMap, "controls.apps1_v"));
    const apps2V = toFiniteNumber(field(values, indexMap, "controls.apps2_v"));
    const bse1V = toFiniteNumber(field(values, indexMap, "controls.bse1_v"));
    const bse2V = toFiniteNumber(field(values, indexMap, "controls.bse2_v"));
    const brakePressure = toFiniteNumber(field(values, indexMap, "controls.brake_pressure_f"));
    const hvPackV = firstNonZero(
      toFiniteNumber(field(values, indexMap, "pack.hv_pack_v")),
      toFiniteNumber(field(values, indexMap, "pack.dc_bus_v")),
      toFiniteNumber(field(values, indexMap, "pack.bus_voltage")),
    );
    const hvCurrent = firstNonZero(
      toFiniteNumber(field(values, indexMap, "pack.hv_c")),
      toFiniteNumber(field(values, indexMap, "pack.dc_bus_current")),
    );
    const hvSocRaw = toFiniteNumber(field(values, indexMap, "pack.hv_soc"));
    const hvSoc =
      (typeof hvSocRaw === "number" && hvSocRaw > 0.1 ? clamp(hvSocRaw, 0, 100) : undefined) ??
      estimateSocFromVoltage(hvPackV);
    const lvBattV = firstNonZero(toFiniteNumber(field(values, indexMap, "pack.lv_batt_v")));
    const steerColAngle = toFiniteNumber(field(values, indexMap, "dynamics.steer_col_angle"));
    const timeSinceOnS =
      firstNonZero(toFiniteNumber(field(values, indexMap, "pack.time_since_on"))) ??
      Math.max(0, (timeMs - trimStartMs) / 1000);
    const inverterTemp = firstNonZero(
      toFiniteNumber(field(values, indexMap, "thermal.inverter_temp")),
      toFiniteNumber(field(values, indexMap, "thermal.inverter_hotspot_temp")),
      toFiniteNumber(field(values, indexMap, "thermal.gate_driver_temp")),
    );
    const motorTemp = firstNonZero(toFiniteNumber(field(values, indexMap, "thermal.motor_temp")));
    const ambientTemp = firstNonZero(
      toFiniteNumber(field(values, indexMap, "thermal.ambient_temp")),
      toFiniteNumber(field(values, indexMap, "thermal.coolant_temp")),
    );

    const flPot = toFiniteNumber(field(values, indexMap, "dynamics.fl_sus_pot_v"));
    const frPot = toFiniteNumber(field(values, indexMap, "dynamics.fr_sus_pot_v"));
    const blPot = toFiniteNumber(field(values, indexMap, "dynamics.bl_sus_pot_v"));
    const brPot = toFiniteNumber(field(values, indexMap, "dynamics.br_sus_pot_v"));
    const cellsTempsRaw = parseNumberArray(field(values, indexMap, "pack.cells_temps"));
    const cellsTemps =
      cellsTempsRaw && cellsTempsRaw.length >= 90 ? cellsTempsRaw.slice(0, 90) : cellsTempsRaw;
    if (!cachedCellTemps && cellsTemps?.length) {
      cachedCellTemps = cellsTemps;
    }

    const throttlePctFromAccel = normalizePct(accelPedal);
    const throttlePctFromTravel = normalizePct(
      Math.max(apps1Travel ?? Number.NEGATIVE_INFINITY, apps2Travel ?? Number.NEGATIVE_INFINITY),
    );
    const throttlePctFromAppsV = appsVoltageToPct(apps1V, apps2V);
    const throttlePct =
      (throttlePctFromAccel !== undefined && throttlePctFromAccel > 0.1
        ? throttlePctFromAccel
        : undefined) ??
      (throttlePctFromTravel !== undefined && throttlePctFromTravel > 0.1
        ? throttlePctFromTravel
        : undefined) ??
      throttlePctFromAppsV;
    const brakePctFromV = brakeVoltageToPct(bse1V, bse2V);
    const brakePctFromPressure = brakePressureToPctInverted(brakePressure);
    const brakePct =
      (brakePctFromV !== undefined && brakePctFromV > 0.1 ? brakePctFromV : undefined) ??
      brakePctFromPressure;
    const resolvedHvPackV = firstFinite(hvPackV, lastHvPackV);
    const resolvedHvCurrent = firstFinite(hvCurrent, lastHvCurrent);
    const resolvedHvSoc = firstFinite(hvSoc, lastHvSoc);
    const resolvedLvBattV = firstFinite(lvBattV, lastLvV);
    const resolvedInverterTemp = firstFinite(inverterTemp, lastInverterTemp);
    const resolvedMotorTemp = firstFinite(motorTemp, lastMotorTemp);
    const resolvedAmbientTemp = firstFinite(ambientTemp, lastAmbientTemp);
    const powerKw =
      resolvedHvPackV !== undefined && resolvedHvCurrent !== undefined
        ? (resolvedHvPackV * resolvedHvCurrent) / 1000
        : undefined;

    lastHvPackV = resolvedHvPackV;
    lastHvCurrent = resolvedHvCurrent;
    lastHvSoc = resolvedHvSoc;
    lastLvV = resolvedLvBattV;
    lastInverterTemp = resolvedInverterTemp;
    lastMotorTemp = resolvedMotorTemp;
    lastAmbientTemp = resolvedAmbientTemp;

    frames.push({
      timestampMs: timeMs,
      dashboard: {
        packetId: packetId ?? null,
        speed: speedMph,
        wheelSpeedAvg: speedMph,
        steerColAngle: steerColAngle ?? null,
        throttlePct: throttlePct ?? null,
        brakePct: brakePct ?? null,
        batteryPct: resolvedHvSoc ?? null,
        hvPackV: resolvedHvPackV ?? null,
        hvCurrent: resolvedHvCurrent ?? null,
        lvV: resolvedLvBattV ?? null,
        inverterTempC: resolvedInverterTemp ?? null,
        motorTempC: resolvedMotorTemp ?? null,
        ambientTempC: resolvedAmbientTemp ?? null,
      },
      liveBanner: {
        battery: resolvedHvSoc ?? null,
        odometer: odometerMiles,
      },
      energy: {
        powerKw: powerKw ?? null,
        timeSinceOnS: timeSinceOnS ?? null,
        batteryPct: resolvedHvSoc ?? null,
      },
      map: {
        dynamics: { gps: lastGps ? [lastGps[0], lastGps[1]] : null },
      },
      sensor: {
        dynamics: {
          gps: lastGps ? [lastGps[0], lastGps[1]] : null,
          fl_sus_pot_v: flPot ?? null,
          fr_sus_pot_v: frPot ?? null,
          bl_sus_pot_v: blPot ?? null,
          br_sus_pot_v: brPot ?? null,
        },
      },
    });

    while (nextSampleMs <= timeMs) {
      nextSampleMs += SAMPLE_MS;
    }
  }

  if (!frames.length) {
    throw new Error(`CSV '${resolvedFileName}' produced no replay frames in trimmed window`);
  }

  return {
    fileName: resolvedFileName,
    sourcePath: filePath,
    sampleMs: SAMPLE_MS,
    trimStartMs,
    trimEndMs,
    durationMs: Math.max(0, trimEndMs - trimStartMs),
    frameCount: frames.length,
    cellTemps: cachedCellTemps,
    frames,
  };
}

export async function loadOrionBackupReplay(fileName?: string): Promise<OrionBackupReplay> {
  const { filePath } = await resolveOrionLogFile(fileName);
  if (!replayCache || replayCacheKey !== filePath) {
    replayCacheKey = filePath;
    replayCache = buildReplay(fileName);
  }
  return replayCache;
}
