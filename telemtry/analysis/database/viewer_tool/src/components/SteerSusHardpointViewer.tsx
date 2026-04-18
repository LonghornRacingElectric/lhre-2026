"use client";

import { OrbitControls } from "@react-three/drei";
import { Canvas } from "@react-three/fiber";
import { useMemo } from "react";
import * as THREE from "three";

type HardpointViewerProps = {
  data?: Record<string, unknown> | null;
  accentColor?: string;
  accentSoftColor?: string;
};

const POINT_SIGNALS = [
  "leftUpperFore_i",
  "leftUpperAft_i",
  "leftLowerFore_i",
  "leftLowerAft_i",
  "leftUpper_o",
  "leftLower_o",
  "leftTie_i",
  "leftTie_o",
  "leftWheelCenter",
  "leftCP",
  "leftBellcrankPivot",
  "leftBellcrankPickup1",
  "leftBellcrankPickup2",
  "leftBellcrankPickup3",
  "leftRodMount",
  "leftShockMount",
  "leftBarEnd",
  "leftArmEnd",
  "rightUpperFore_i",
  "rightUpperAft_i",
  "rightLowerFore_i",
  "rightLowerAft_i",
  "rightUpper_o",
  "rightLower_o",
  "rightTie_i",
  "rightTie_o",
  "rightWheelCenter",
  "rightCP",
  "rightBellcrankPivot",
  "rightBellcrankPickup1",
  "rightBellcrankPickup2",
  "rightBellcrankPickup3",
  "rightRodMount",
  "rightShockMount",
  "rightBarEnd",
  "rightArmEnd",
  "rearLeftUpperFore_i",
  "rearLeftUpperAft_i",
  "rearLeftLowerFore_i",
  "rearLeftLowerAft_i",
  "rearLeftUpper_o",
  "rearLeftLower_o",
  "rearLeftTie_i",
  "rearLeftTie_o",
  "rearLeftWheelCenter",
  "rearLeftCP",
  "rearLeftBellcrankPivot",
  "rearLeftBellcrankPickup1",
  "rearLeftBellcrankPickup2",
  "rearLeftBellcrankPickup3",
  "rearLeftRodMount",
  "rearLeftShockMount",
  "rearLeftBarEnd",
  "rearLeftArmEnd",
  "rearRightUpperFore_i",
  "rearRightUpperAft_i",
  "rearRightLowerFore_i",
  "rearRightLowerAft_i",
  "rearRightUpper_o",
  "rearRightLower_o",
  "rearRightTie_i",
  "rearRightTie_o",
  "rearRightWheelCenter",
  "rearRightCP",
  "rearRightBellcrankPivot",
  "rearRightBellcrankPickup1",
  "rearRightBellcrankPickup2",
  "rearRightBellcrankPickup3",
  "rearRightRodMount",
  "rearRightShockMount",
  "rearRightBarEnd",
  "rearRightArmEnd",
] as const;

const DIRECTION_SIGNALS = [
  "leftTire_ex",
  "leftTire_ey",
  "rightTire_ex",
  "rightTire_ey",
  "rearLeftTire_ex",
  "rearLeftTire_ey",
  "rearRightTire_ex",
  "rearRightTire_ey",
] as const;

const FORCE_SIGNALS = ["leftCPForce", "rightCPForce", "rearLeftCPForce", "rearRightCPForce"] as const;

const ALL_SIGNALS = [...POINT_SIGNALS, ...DIRECTION_SIGNALS, ...FORCE_SIGNALS] as const;

const POINT_SET = new Set<string>(POINT_SIGNALS);
const DIRECTION_SET = new Set<string>(DIRECTION_SIGNALS);

const LINK_DEFINITIONS: Array<{ from: string; to: string; side: "left" | "right" | "neutral" }> = [
  { from: "leftUpperFore_i", to: "leftUpper_o", side: "left" },
  { from: "leftUpperAft_i", to: "leftUpper_o", side: "left" },
  { from: "leftLowerFore_i", to: "leftLower_o", side: "left" },
  { from: "leftLowerAft_i", to: "leftLower_o", side: "left" },
  { from: "leftUpper_o", to: "leftLower_o", side: "left" },
  { from: "leftTie_i", to: "leftTie_o", side: "left" },
  { from: "leftLower_o", to: "leftWheelCenter", side: "left" },
  { from: "leftUpper_o", to: "leftWheelCenter", side: "left" },
  { from: "leftWheelCenter", to: "leftCP", side: "left" },
  { from: "leftBellcrankPivot", to: "leftBellcrankPickup1", side: "left" },
  { from: "leftBellcrankPivot", to: "leftBellcrankPickup2", side: "left" },
  { from: "leftBellcrankPivot", to: "leftBellcrankPickup3", side: "left" },
  { from: "leftBellcrankPickup2", to: "leftRodMount", side: "left" },
  { from: "leftBellcrankPickup3", to: "leftShockMount", side: "left" },
  { from: "leftBellcrankPickup1", to: "leftArmEnd", side: "left" },
  { from: "leftArmEnd", to: "leftBarEnd", side: "left" },
  { from: "rightUpperFore_i", to: "rightUpper_o", side: "right" },
  { from: "rightUpperAft_i", to: "rightUpper_o", side: "right" },
  { from: "rightLowerFore_i", to: "rightLower_o", side: "right" },
  { from: "rightLowerAft_i", to: "rightLower_o", side: "right" },
  { from: "rightUpper_o", to: "rightLower_o", side: "right" },
  { from: "rightTie_i", to: "rightTie_o", side: "right" },
  { from: "rightLower_o", to: "rightWheelCenter", side: "right" },
  { from: "rightUpper_o", to: "rightWheelCenter", side: "right" },
  { from: "rightWheelCenter", to: "rightCP", side: "right" },
  { from: "rightBellcrankPivot", to: "rightBellcrankPickup1", side: "right" },
  { from: "rightBellcrankPivot", to: "rightBellcrankPickup2", side: "right" },
  { from: "rightBellcrankPivot", to: "rightBellcrankPickup3", side: "right" },
  { from: "rightBellcrankPickup2", to: "rightRodMount", side: "right" },
  { from: "rightBellcrankPickup3", to: "rightShockMount", side: "right" },
  { from: "rightBellcrankPickup1", to: "rightArmEnd", side: "right" },
  { from: "rightArmEnd", to: "rightBarEnd", side: "right" },
  { from: "rearLeftUpperFore_i", to: "rearLeftUpper_o", side: "left" },
  { from: "rearLeftUpperAft_i", to: "rearLeftUpper_o", side: "left" },
  { from: "rearLeftLowerFore_i", to: "rearLeftLower_o", side: "left" },
  { from: "rearLeftLowerAft_i", to: "rearLeftLower_o", side: "left" },
  { from: "rearLeftUpper_o", to: "rearLeftLower_o", side: "left" },
  { from: "rearLeftTie_i", to: "rearLeftTie_o", side: "left" },
  { from: "rearLeftLower_o", to: "rearLeftWheelCenter", side: "left" },
  { from: "rearLeftUpper_o", to: "rearLeftWheelCenter", side: "left" },
  { from: "rearLeftWheelCenter", to: "rearLeftCP", side: "left" },
  { from: "rearLeftBellcrankPivot", to: "rearLeftBellcrankPickup1", side: "left" },
  { from: "rearLeftBellcrankPivot", to: "rearLeftBellcrankPickup2", side: "left" },
  { from: "rearLeftBellcrankPivot", to: "rearLeftBellcrankPickup3", side: "left" },
  { from: "rearLeftBellcrankPickup2", to: "rearLeftRodMount", side: "left" },
  { from: "rearLeftBellcrankPickup3", to: "rearLeftShockMount", side: "left" },
  { from: "rearLeftBellcrankPickup1", to: "rearLeftArmEnd", side: "left" },
  { from: "rearLeftArmEnd", to: "rearLeftBarEnd", side: "left" },
  { from: "rearRightUpperFore_i", to: "rearRightUpper_o", side: "right" },
  { from: "rearRightUpperAft_i", to: "rearRightUpper_o", side: "right" },
  { from: "rearRightLowerFore_i", to: "rearRightLower_o", side: "right" },
  { from: "rearRightLowerAft_i", to: "rearRightLower_o", side: "right" },
  { from: "rearRightUpper_o", to: "rearRightLower_o", side: "right" },
  { from: "rearRightTie_i", to: "rearRightTie_o", side: "right" },
  { from: "rearRightLower_o", to: "rearRightWheelCenter", side: "right" },
  { from: "rearRightUpper_o", to: "rearRightWheelCenter", side: "right" },
  { from: "rearRightWheelCenter", to: "rearRightCP", side: "right" },
  { from: "rearRightBellcrankPivot", to: "rearRightBellcrankPickup1", side: "right" },
  { from: "rearRightBellcrankPivot", to: "rearRightBellcrankPickup2", side: "right" },
  { from: "rearRightBellcrankPivot", to: "rearRightBellcrankPickup3", side: "right" },
  { from: "rearRightBellcrankPickup2", to: "rearRightRodMount", side: "right" },
  { from: "rearRightBellcrankPickup3", to: "rearRightShockMount", side: "right" },
  { from: "rearRightBellcrankPickup1", to: "rearRightArmEnd", side: "right" },
  { from: "rearRightArmEnd", to: "rearRightBarEnd", side: "right" },
];

const TIRE_DEFINITIONS = [
  {
    center: "leftWheelCenter",
    ex: "leftTire_ex",
    ey: "leftTire_ey",
    side: "left" as const,
  },
  {
    center: "rightWheelCenter",
    ex: "rightTire_ex",
    ey: "rightTire_ey",
    side: "right" as const,
  },
  {
    center: "rearLeftWheelCenter",
    ex: "rearLeftTire_ex",
    ey: "rearLeftTire_ey",
    side: "left" as const,
  },
  {
    center: "rearRightWheelCenter",
    ex: "rearRightTire_ex",
    ey: "rearRightTire_ey",
    side: "right" as const,
  },
];

const FALLBACK_SIGNAL_COORDS: Record<string, [number, number, number]> = {
  leftUpperFore_i: [-0.55, 0.48, 0.58],
  leftUpperAft_i: [-0.55, 0.47, 0.34],
  leftLowerFore_i: [-0.6, 0.24, 0.58],
  leftLowerAft_i: [-0.6, 0.24, 0.34],
  leftUpper_o: [-0.89, 0.43, 0.47],
  leftLower_o: [-0.92, 0.2, 0.46],
  leftTie_i: [-0.49, 0.33, 0.52],
  leftTie_o: [-0.9, 0.31, 0.49],
  leftWheelCenter: [-1.02, 0.29, 0.46],
  leftTire_ex: [0, 0, 1],
  leftTire_ey: [1, 0, 0],
  leftCP: [-1.02, 0.02, 0.46],
  leftCPForce: [0, 1, 0],
  leftBellcrankPivot: [-0.42, 0.58, 0.39],
  leftBellcrankPickup1: [-0.48, 0.51, 0.35],
  leftBellcrankPickup2: [-0.45, 0.53, 0.43],
  leftBellcrankPickup3: [-0.39, 0.52, 0.36],
  leftRodMount: [-0.58, 0.45, 0.38],
  leftShockMount: [-0.34, 0.7, 0.34],
  leftBarEnd: [-0.52, 0.42, 0.2],
  leftArmEnd: [-0.45, 0.4, 0.2],
  rightUpperFore_i: [0.55, 0.48, 0.58],
  rightUpperAft_i: [0.55, 0.47, 0.34],
  rightLowerFore_i: [0.6, 0.24, 0.58],
  rightLowerAft_i: [0.6, 0.24, 0.34],
  rightUpper_o: [0.89, 0.43, 0.47],
  rightLower_o: [0.92, 0.2, 0.46],
  rightTie_i: [0.49, 0.33, 0.52],
  rightTie_o: [0.9, 0.31, 0.49],
  rightWheelCenter: [1.02, 0.29, 0.46],
  rightTire_ex: [0, 0, 1],
  rightTire_ey: [1, 0, 0],
  rightCP: [1.02, 0.02, 0.46],
  rightCPForce: [0, 1, 0],
  rightBellcrankPivot: [0.42, 0.58, 0.39],
  rightBellcrankPickup1: [0.48, 0.51, 0.35],
  rightBellcrankPickup2: [0.45, 0.53, 0.43],
  rightBellcrankPickup3: [0.39, 0.52, 0.36],
  rightRodMount: [0.58, 0.45, 0.38],
  rightShockMount: [0.34, 0.7, 0.34],
  rightBarEnd: [0.52, 0.42, 0.2],
  rightArmEnd: [0.45, 0.4, 0.2],
  rearLeftUpperFore_i: [-0.52, 0.48, -0.52],
  rearLeftUpperAft_i: [-0.52, 0.47, -0.78],
  rearLeftLowerFore_i: [-0.58, 0.24, -0.52],
  rearLeftLowerAft_i: [-0.58, 0.24, -0.78],
  rearLeftUpper_o: [-0.98, 0.43, -0.65],
  rearLeftLower_o: [-1.01, 0.2, -0.65],
  rearLeftTie_i: [-0.46, 0.32, -0.58],
  rearLeftTie_o: [-0.99, 0.31, -0.67],
  rearLeftWheelCenter: [-1.18, 0.28, -0.7],
  rearLeftTire_ex: [0, 0, 1],
  rearLeftTire_ey: [1, 0, 0],
  rearLeftCP: [-1.18, 0.02, -0.7],
  rearLeftCPForce: [0, 1, 0],
  rearLeftBellcrankPivot: [-0.4, 0.58, -0.72],
  rearLeftBellcrankPickup1: [-0.46, 0.51, -0.76],
  rearLeftBellcrankPickup2: [-0.43, 0.53, -0.67],
  rearLeftBellcrankPickup3: [-0.37, 0.52, -0.74],
  rearLeftRodMount: [-0.56, 0.45, -0.71],
  rearLeftShockMount: [-0.32, 0.7, -0.76],
  rearLeftBarEnd: [-0.5, 0.42, -0.92],
  rearLeftArmEnd: [-0.43, 0.4, -0.92],
  rearRightUpperFore_i: [0.52, 0.48, -0.52],
  rearRightUpperAft_i: [0.52, 0.47, -0.78],
  rearRightLowerFore_i: [0.58, 0.24, -0.52],
  rearRightLowerAft_i: [0.58, 0.24, -0.78],
  rearRightUpper_o: [0.98, 0.43, -0.65],
  rearRightLower_o: [1.01, 0.2, -0.65],
  rearRightTie_i: [0.46, 0.32, -0.58],
  rearRightTie_o: [0.99, 0.31, -0.67],
  rearRightWheelCenter: [1.18, 0.28, -0.7],
  rearRightTire_ex: [0, 0, 1],
  rearRightTire_ey: [1, 0, 0],
  rearRightCP: [1.18, 0.02, -0.7],
  rearRightCPForce: [0, 1, 0],
  rearRightBellcrankPivot: [0.4, 0.58, -0.72],
  rearRightBellcrankPickup1: [0.46, 0.51, -0.76],
  rearRightBellcrankPickup2: [0.43, 0.53, -0.67],
  rearRightBellcrankPickup3: [0.37, 0.52, -0.74],
  rearRightRodMount: [0.56, 0.45, -0.71],
  rearRightShockMount: [0.32, 0.7, -0.76],
  rearRightBarEnd: [0.5, 0.42, -0.92],
  rearRightArmEnd: [0.43, 0.4, -0.92],
};

const LEFT_COLOR = "#fb923c";
const RIGHT_COLOR = "#60a5fa";
const NEUTRAL_COLOR = "#cbd5e1";
const EPSILON = 1e-7;
const NESTED_SIGNAL_PATHS: Record<string, string[]> = {
  rearLeftUpperFore_i: ["geometry", "rear", "left", "upperFore_i"],
  rearLeftUpperAft_i: ["geometry", "rear", "left", "upperAft_i"],
  rearLeftLowerFore_i: ["geometry", "rear", "left", "lowerFore_i"],
  rearLeftLowerAft_i: ["geometry", "rear", "left", "lowerAft_i"],
  rearLeftUpper_o: ["geometry", "rear", "left", "upper_o"],
  rearLeftLower_o: ["geometry", "rear", "left", "lower_o"],
  rearLeftTie_i: ["geometry", "rear", "left", "tie_i"],
  rearLeftTie_o: ["geometry", "rear", "left", "tie_o"],
  rearLeftWheelCenter: ["geometry", "rear", "left", "wheelCenter"],
  rearLeftTire_ex: ["geometry", "rear", "left", "tire_ex"],
  rearLeftTire_ey: ["geometry", "rear", "left", "tire_ey"],
  rearLeftCP: ["geometry", "rear", "left", "CP"],
  rearLeftCPForce: ["geometry", "rear", "left", "CPForce"],
  rearLeftBellcrankPivot: ["geometry", "rear", "left", "bellcrankPivot"],
  rearLeftBellcrankPickup1: ["geometry", "rear", "left", "bellcrankPickup1"],
  rearLeftBellcrankPickup2: ["geometry", "rear", "left", "bellcrankPickup2"],
  rearLeftBellcrankPickup3: ["geometry", "rear", "left", "bellcrankPickup3"],
  rearLeftRodMount: ["geometry", "rear", "left", "rodMount"],
  rearLeftShockMount: ["geometry", "rear", "left", "shockMount"],
  rearLeftBarEnd: ["geometry", "rear", "left", "barEnd"],
  rearLeftArmEnd: ["geometry", "rear", "left", "armEnd"],
  rearRightUpperFore_i: ["geometry", "rear", "right", "upperFore_i"],
  rearRightUpperAft_i: ["geometry", "rear", "right", "upperAft_i"],
  rearRightLowerFore_i: ["geometry", "rear", "right", "lowerFore_i"],
  rearRightLowerAft_i: ["geometry", "rear", "right", "lowerAft_i"],
  rearRightUpper_o: ["geometry", "rear", "right", "upper_o"],
  rearRightLower_o: ["geometry", "rear", "right", "lower_o"],
  rearRightTie_i: ["geometry", "rear", "right", "tie_i"],
  rearRightTie_o: ["geometry", "rear", "right", "tie_o"],
  rearRightWheelCenter: ["geometry", "rear", "right", "wheelCenter"],
  rearRightTire_ex: ["geometry", "rear", "right", "tire_ex"],
  rearRightTire_ey: ["geometry", "rear", "right", "tire_ey"],
  rearRightCP: ["geometry", "rear", "right", "CP"],
  rearRightCPForce: ["geometry", "rear", "right", "CPForce"],
  rearRightBellcrankPivot: ["geometry", "rear", "right", "bellcrankPivot"],
  rearRightBellcrankPickup1: ["geometry", "rear", "right", "bellcrankPickup1"],
  rearRightBellcrankPickup2: ["geometry", "rear", "right", "bellcrankPickup2"],
  rearRightBellcrankPickup3: ["geometry", "rear", "right", "bellcrankPickup3"],
  rearRightRodMount: ["geometry", "rear", "right", "rodMount"],
  rearRightShockMount: ["geometry", "rear", "right", "shockMount"],
  rearRightBarEnd: ["geometry", "rear", "right", "barEnd"],
  rearRightArmEnd: ["geometry", "rear", "right", "armEnd"],
};

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function toFiniteNumber(value: unknown): number | undefined {
  const parsed = typeof value === "number" ? value : Number(value);
  return Number.isFinite(parsed) ? parsed : undefined;
}

function toVector3(value: unknown): THREE.Vector3 | null {
  if (Array.isArray(value) && value.length >= 3) {
    const x = toFiniteNumber(value[0]);
    const y = toFiniteNumber(value[1]);
    const z = toFiniteNumber(value[2]);
    if (x !== undefined && y !== undefined && z !== undefined) {
      return new THREE.Vector3(x, y, z);
    }
  }

  if (isRecord(value)) {
    const x = toFiniteNumber(value.x ?? value["0"]);
    const y = toFiniteNumber(value.y ?? value["1"]);
    const z = toFiniteNumber(value.z ?? value["2"]);
    if (x !== undefined && y !== undefined && z !== undefined) {
      return new THREE.Vector3(x, y, z);
    }
  }

  return null;
}

function vectorFromIndexedComponents(record: Record<string, unknown>, key: string): THREE.Vector3 | null {
  const keyPatterns = [
    [`${key}[0]`, `${key}[1]`, `${key}[2]`],
    [`${key}[1]`, `${key}[2]`, `${key}[3]`],
    [`${key}_0`, `${key}_1`, `${key}_2`],
    [`${key}_x`, `${key}_y`, `${key}_z`],
  ];

  for (const [xKey, yKey, zKey] of keyPatterns) {
    const x = toFiniteNumber(record[xKey]);
    const y = toFiniteNumber(record[yKey]);
    const z = toFiniteNumber(record[zKey]);
    if (x !== undefined && y !== undefined && z !== undefined) {
      return new THREE.Vector3(x, y, z);
    }
  }

  return null;
}

function findVectorDeep(root: unknown, key: string): THREE.Vector3 | null {
  if (!isRecord(root)) return null;

  const stack: Array<Record<string, unknown>> = [root];
  const visited = new Set<Record<string, unknown>>();

  while (stack.length > 0) {
    const node = stack.pop();
    if (!node || visited.has(node)) continue;
    visited.add(node);

    if (Object.prototype.hasOwnProperty.call(node, key)) {
      const direct = toVector3(node[key]);
      if (direct) return direct;
    }

    const indexed = vectorFromIndexedComponents(node, key);
    if (indexed) return indexed;

    for (const value of Object.values(node)) {
      if (isRecord(value)) {
        stack.push(value);
      } else if (Array.isArray(value)) {
        for (const entry of value) {
          if (isRecord(entry)) {
            stack.push(entry);
          }
        }
      }
    }
  }

  return null;
}

function tupleToVector3(tuple: [number, number, number]): THREE.Vector3 {
  return new THREE.Vector3(tuple[0], tuple[1], tuple[2]);
}

function getValueAtPath(root: unknown, path: string[]): unknown {
  let current: unknown = root;
  for (const segment of path) {
    if (!isRecord(current) || !(segment in current)) {
      return undefined;
    }
    current = current[segment];
  }
  return current;
}

function vecToTuple(vec: THREE.Vector3): [number, number, number] {
  return [vec.x, vec.y, vec.z];
}

function buildSceneSignals(payload: Record<string, unknown> | null | undefined): {
  signals: Record<string, THREE.Vector3>;
  liveCount: number;
} {
  const signals: Record<string, THREE.Vector3> = {};
  let liveCount = 0;

  for (const key of ALL_SIGNALS) {
    const live = payload ? findVectorDeep(payload, key) : null;
    if (live) {
      signals[key] = live;
      liveCount += 1;
      continue;
    }

    const nestedPath = NESTED_SIGNAL_PATHS[key];
    if (payload && nestedPath) {
      const nested = toVector3(getValueAtPath(payload, nestedPath));
      if (nested) {
        signals[key] = nested;
        liveCount += 1;
        continue;
      }
    }

    const fallback = FALLBACK_SIGNAL_COORDS[key];
    if (fallback) {
      signals[key] = tupleToVector3(fallback);
    }
  }

  return { signals, liveCount };
}

function normalizePointSignals(signals: Record<string, THREE.Vector3>): Record<string, THREE.Vector3> {
  const pointEntries = Object.entries(signals).filter(([name]) => POINT_SET.has(name));
  if (!pointEntries.length) return signals;

  const bounds = new THREE.Box3();
  for (const [, value] of pointEntries) {
    bounds.expandByPoint(value);
  }

  const center = bounds.getCenter(new THREE.Vector3());
  const size = bounds.getSize(new THREE.Vector3());
  const span = Math.max(size.x, size.y, size.z, 0.25);
  const scale = 3.5 / span;

  const normalized: Record<string, THREE.Vector3> = {};
  for (const [name, value] of Object.entries(signals)) {
    if (POINT_SET.has(name)) {
      normalized[name] = value.clone().sub(center).multiplyScalar(scale);
    } else {
      normalized[name] = value.clone();
    }
  }

  return normalized;
}

function linkColor(side: "left" | "right" | "neutral"): string {
  if (side === "left") return LEFT_COLOR;
  if (side === "right") return RIGHT_COLOR;
  return NEUTRAL_COLOR;
}

function pointColor(signalName: string): string {
  const normalized = signalName.toLowerCase();
  if (normalized.startsWith("left") || normalized.includes("left")) return LEFT_COLOR;
  if (normalized.startsWith("right") || normalized.includes("right")) return RIGHT_COLOR;
  return NEUTRAL_COLOR;
}

function LinkCylinder({
  start,
  end,
  radius,
  color,
}: {
  start: THREE.Vector3;
  end: THREE.Vector3;
  radius: number;
  color: string;
}) {
  const { position, quaternion, length } = useMemo(() => {
    const delta = end.clone().sub(start);
    const length = delta.length();
    const midpoint = start.clone().add(end).multiplyScalar(0.5);
    const rotation = new THREE.Quaternion();

    if (length > EPSILON) {
      const direction = delta.normalize();
      rotation.setFromUnitVectors(new THREE.Vector3(0, 1, 0), direction);
    }

    return {
      position: vecToTuple(midpoint),
      quaternion: rotation,
      length,
    };
  }, [end, start]);

  if (length <= EPSILON) return null;

  return (
    <mesh position={position} quaternion={quaternion}>
      <cylinderGeometry args={[radius, radius, length, 12]} />
      <meshStandardMaterial color={color} />
    </mesh>
  );
}

function TirePrimitive({
  center,
  ex,
  ey,
  color,
}: {
  center: THREE.Vector3;
  ex?: THREE.Vector3;
  ey?: THREE.Vector3;
  color: string;
}) {
  const quaternion = useMemo(() => {
    if (!ex || !ey) {
      return new THREE.Quaternion();
    }

    const xAxis = ex.clone().normalize();
    const yAxis = ey.clone().normalize();
    if (xAxis.lengthSq() < EPSILON || yAxis.lengthSq() < EPSILON) {
      return new THREE.Quaternion();
    }

    const zAxis = new THREE.Vector3().crossVectors(xAxis, yAxis);
    if (zAxis.lengthSq() < EPSILON) {
      return new THREE.Quaternion();
    }

    zAxis.normalize();
    yAxis.crossVectors(zAxis, xAxis).normalize();

    const basis = new THREE.Matrix4().makeBasis(xAxis, yAxis, zAxis);
    return new THREE.Quaternion().setFromRotationMatrix(basis);
  }, [ex, ey]);

  return (
    <group position={vecToTuple(center)} quaternion={quaternion}>
      <mesh>
        <cylinderGeometry args={[0.38, 0.38, 0.2, 24]} />
        <meshStandardMaterial color="#0f172a" />
      </mesh>
      <mesh>
        <cylinderGeometry args={[0.24, 0.24, 0.21, 20]} />
        <meshStandardMaterial color={color} metalness={0.25} roughness={0.4} />
      </mesh>
    </group>
  );
}

export default function SteerSusHardpointViewer({
  data,
  accentColor = "#BF5700",
  accentSoftColor = "#FFD2B0",
}: HardpointViewerProps) {
  const { normalizedSignals, liveCount } = useMemo(() => {
    const built = buildSceneSignals(data);
    return {
      normalizedSignals: normalizePointSignals(built.signals),
      liveCount: built.liveCount,
    };
  }, [data]);

  const pointEntries = useMemo(
    () => Object.entries(normalizedSignals).filter(([name]) => POINT_SET.has(name)),
    [normalizedSignals],
  );
  const steerLinkEntries = useMemo(
    () =>
      LINK_DEFINITIONS.filter(
        ({ from, to }) =>
          normalizedSignals[from] !== undefined &&
          normalizedSignals[to] !== undefined,
      ),
    [normalizedSignals],
  );

  return (
    <div className="relative h-full w-full">
      <Canvas camera={{ position: [0, 2.2, 5.2], fov: 44 }}>
        <color attach="background" args={["#050507"]} />
        <ambientLight intensity={0.45} />
        <directionalLight position={[3, 4, 2]} intensity={1.05} color={accentSoftColor} />
        <directionalLight position={[-3, 2, -2]} intensity={0.55} color={accentColor} />

        <gridHelper args={[8, 16, "#374151", "#1f2937"]} position={[0, -1.4, 0]} />

        {steerLinkEntries.map(({ from, to, side }) => (
          <LinkCylinder
            key={`${from}-${to}`}
            start={normalizedSignals[from]}
            end={normalizedSignals[to]}
            radius={0.03}
            color={linkColor(side)}
          />
        ))}

        {pointEntries.map(([name, value]) => (
          <mesh key={name} position={vecToTuple(value)}>
            <sphereGeometry args={[name.includes("WheelCenter") ? 0.085 : 0.055, 14, 14]} />
            <meshStandardMaterial color={pointColor(name)} />
          </mesh>
        ))}

        {TIRE_DEFINITIONS.map(({ center, ex, ey, side }) => {
          const wheelCenter = normalizedSignals[center];
          if (!wheelCenter) return null;

          const exDir = normalizedSignals[ex];
          const eyDir = normalizedSignals[ey];
          const tireColor = side === "left" ? accentColor : accentSoftColor;
          return (
            <TirePrimitive
              key={center}
              center={wheelCenter}
              ex={DIRECTION_SET.has(ex) ? exDir : undefined}
              ey={DIRECTION_SET.has(ey) ? eyDir : undefined}
              color={tireColor}
            />
          );
        })}

        {FORCE_SIGNALS.map((forceSignal) => {
          const cpSignal = forceSignal.replace("CPForce", "CP");
          const cp = normalizedSignals[cpSignal];
          const force = normalizedSignals[forceSignal];
          if (!cp || !force || force.lengthSq() < EPSILON) return null;

          const direction = force.clone().normalize();
          const end = cp.clone().add(direction.multiplyScalar(0.6));
          return (
            <LinkCylinder
              key={`${forceSignal}-arrow`}
                start={cp}
                end={end}
                radius={0.015}
                color={forceSignal.toLowerCase().includes("left") ? LEFT_COLOR : RIGHT_COLOR}
              />
            );
          })}

        <OrbitControls
          enableDamping
          dampingFactor={0.08}
          target={[0, 0.2, 0.2]}
          maxPolarAngle={Math.PI * 0.49}
        />
      </Canvas>

      <div className="pointer-events-none absolute left-3 top-3 rounded-md border border-white/20 bg-black/50 px-3 py-1 text-xs text-white/90">
        {liveCount > 0
          ? `steer_sus vectors live: ${liveCount}/${ALL_SIGNALS.length}`
          : "No steer_sus vectors yet — showing reference hardpoints"}
      </div>
    </div>
  );
}
