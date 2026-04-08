"use client";

import { Canvas, useFrame, useLoader } from "@react-three/fiber";
import { OrbitControls } from "@react-three/drei";
// STLLoader lives in three/examples; import at runtime via useLoader below
import { useRef } from "react";
import * as THREE from "three";
import React from "react";
import { useKafkaJSON } from "@/hooks/useKafkaStream";
import { useCarSelection } from "@/lib/carSelection";

const CarConstants = {
  suspension: {
    rest_length: 0.5,
    max_compression: 0.3,
    max_extension: 0.5,
  },
  wheel: {
    min_speed: 0,
    max_speed: 100,
    min_angle: -Math.PI / 3,
    max_angle: Math.PI / 3,
  },
};

type CarState = {
  fl_wheel_angle: number;
  fr_wheel_angle: number;
  fl_spring_displace: number;
  fr_spring_displace: number;
  bl_spring_displace: number;
  br_spring_displace: number;
};

type CarData = {
  dynamics?: {
    flwSpeed?: number;
    frwSpeed?: number;
    blwSpeed?: number;
    brwSpeed?: number;
  };
};

export type CarVisualizationData = CarData;

export default function CarVisualization({
  data,
}: {
  data?: CarVisualizationData | null;
} = {}) {
  const { selectedCar, ssePath, matchesSelectedCar } = useCarSelection();
  const { data: liveData } = useKafkaJSON<CarData>({
    topic: 'car_visualization',
    car: selectedCar,
    ssePath,
    filter: matchesSelectedCar,
    // Extend staleness so we keep last sample between slower updates
    staleAfterMs: 1000,
    merge: true,
  });

  const carData = data !== undefined ? data : liveData;

  // Normalize speeds (guard against undefined / string / NaN) and apply a visual scaling factor.
  const speedScale = -1; // tweak for visual rotation rate
  // Support both nested (dynamics.flw_speed) and flat (flw_speed) routing outputs.
  const flSpeedRaw = carData?.dynamics?.flwSpeed ?? 0;
  const frSpeedRaw = carData?.dynamics?.frwSpeed ?? 0;
  const blSpeedRaw = carData?.dynamics?.blwSpeed ?? 0;
  const brSpeedRaw = carData?.dynamics?.brwSpeed ?? 0;

  const flSpeed = Number(flSpeedRaw) || 0;
  const frSpeed = Number(frSpeedRaw) || 0;
  const blSpeed = Number(blSpeedRaw) || 0;
  const brSpeed = Number(brSpeedRaw) || 0;
  // Removed per-message onMessage and counters to avoid side-effects during streaming

  // const carData = {
  //   flw_speed: 0,
  //   frw_speed: 0,
  //   blw_speed: 0,
  //   brw_speed: 0,
  // };

  const [carState, setCarState] = React.useState<CarState>({
    fl_wheel_angle: 0,
    fr_wheel_angle: 0,
    fl_spring_displace: 0,
    fr_spring_displace: 0,
    bl_spring_displace: 0,
    br_spring_displace: 0,
  });

  // Persistent wheel rotation refs (lifted up)
  const flWheelRot = React.useRef(0);
  const frWheelRot = React.useRef(0);
  const blWheelRot = React.useRef(0);
  const brWheelRot = React.useRef(0);

  return (
    <div className="h-full flex flex-col">
      <div className="flex gap-4 mb-2 overflow-x-auto items-start">
        {/* ...existing code for controls... */}
        <label>
          FR Wheel Speed
          {frSpeed}
        </label>
        <label>
          FL Wheel Speed
          {flSpeed}
        </label>
        <label>
          BR Wheel Speed
          {brSpeed}
        </label>
        <label>
          BL Wheel Speed
          {blSpeed}
        </label>

        <label>
          FL Wheel Angle
          <input
            type="range"
            min={CarConstants.wheel.min_angle}
            max={CarConstants.wheel.max_angle}
            step={0.01}
            value={carState.fl_wheel_angle}
            onChange={(e) =>
              setCarState((s) => ({
                ...s,
                fl_wheel_angle: Number(e.target.value),
              }))
            }
          />
          {carState.fl_wheel_angle.toFixed(2)}
        </label>
        <label>
          FR Wheel Angle
          <input
            type="range"
            min={CarConstants.wheel.min_angle}
            max={CarConstants.wheel.max_angle}
            step={0.01}
            value={carState.fr_wheel_angle}
            onChange={(e) =>
              setCarState((s) => ({
                ...s,
                fr_wheel_angle: Number(e.target.value),
              }))
            }
          />
          {carState.fr_wheel_angle.toFixed(2)}
        </label>
        <label>
          FL Suspension
          <input
            type="range"
            min={-CarConstants.suspension.max_compression}
            max={CarConstants.suspension.max_extension}
            step={0.01}
            value={carState.fl_spring_displace}
            onChange={(e) =>
              setCarState((s) => ({
                ...s,
                fl_spring_displace: Number(e.target.value),
              }))
            }
          />
          {carState.fl_spring_displace.toFixed(2)}
        </label>
        <label>
          FR Suspension
          <input
            type="range"
            min={-CarConstants.suspension.max_compression}
            max={CarConstants.suspension.max_extension}
            step={0.01}
            value={carState.fr_spring_displace}
            onChange={(e) =>
              setCarState((s) => ({
                ...s,
                fr_spring_displace: Number(e.target.value),
              }))
            }
          />
          {carState.fr_spring_displace.toFixed(2)}
        </label>
        <label>
          BL Suspension
          <input
            type="range"
            min={-CarConstants.suspension.max_compression}
            max={CarConstants.suspension.max_extension}
            step={0.01}
            value={carState.bl_spring_displace}
            onChange={(e) =>
              setCarState((s) => ({
                ...s,
                bl_spring_displace: Number(e.target.value),
              }))
            }
          />
          {carState.bl_spring_displace.toFixed(2)}
        </label>
        <label>
          BR Suspension
          <input
            type="range"
            min={-CarConstants.suspension.max_compression}
            max={CarConstants.suspension.max_extension}
            step={0.01}
            value={carState.br_spring_displace}
            onChange={(e) =>
              setCarState((s) => ({
                ...s,
                br_spring_displace: Number(e.target.value),
              }))
            }
          />
          {carState.br_spring_displace.toFixed(2)}
        </label>
      </div>
      <div className="flex-grow min-h-0">
        <Canvas camera={{ position: [0, 5, 12], fov: 50 }}>
          <ambientLight intensity={0.5} />
          <directionalLight position={[5, 10, 5]} intensity={1} />
          <SuspensionSystem
            {...carState}
            flSpeed={flSpeed * speedScale}
            frSpeed={frSpeed * speedScale}
            blSpeed={blSpeed * speedScale}
            brSpeed={brSpeed * speedScale}
            flWheelRot={flWheelRot}
            frWheelRot={frWheelRot}
            blWheelRot={blWheelRot}
            brWheelRot={brWheelRot}
          />
          <OrbitControls />
        </Canvas>
      </div>
    </div>
  );

  function SuspensionSystem({
    flSpeed,
    frSpeed,
    blSpeed,
    brSpeed,
    fl_wheel_angle,
    fr_wheel_angle,
    fl_spring_displace,
    fr_spring_displace,
    bl_spring_displace,
    br_spring_displace,
    flWheelRot,
    frWheelRot,
    blWheelRot,
    brWheelRot,
  }: CarState & {
    flSpeed: number;
    frSpeed: number;
    blSpeed: number;
    brSpeed: number;
    flWheelRot: React.RefObject<number>;
    frWheelRot: React.RefObject<number>;
    blWheelRot: React.RefObject<number>;
    brWheelRot: React.RefObject<number>;
  }) {
    // Groups for front wheels (steering pivots)
    const flPivot = useRef<THREE.Group>(null);
    const frPivot = useRef<THREE.Group>(null);

    // Wheel meshes (for rolling)
    const flWheel = useRef<THREE.Mesh>(null);
    const frWheel = useRef<THREE.Mesh>(null);
    const blWheel = useRef<THREE.Mesh>(null);
    const brWheel = useRef<THREE.Mesh>(null);
    const carBody = useRef<THREE.Mesh>(null);

    // Car constant spring rest length
    const restLength = CarConstants.suspension.rest_length;
    // Actual spring length for each corner
    const flSpringLength = restLength + fl_spring_displace;
    const frSpringLength = restLength + fr_spring_displace;
    const blSpringLength = restLength + bl_spring_displace;
    const brSpringLength = restLength + br_spring_displace;

    useFrame((_, delta) => {
      // Compute per-wheel rotation increments (negative to match original direction)
      const flInc = -(flSpeed ?? 0) * delta;
      const frInc = -(frSpeed ?? 0) * delta;
      const blInc = -(blSpeed ?? 0) * delta;
      const brInc = -(brSpeed ?? 0) * delta;

      // Roll wheels (local X axis), persist rotation independently
      flWheelRot.current += flInc;
      frWheelRot.current += frInc;
      blWheelRot.current += blInc;
      brWheelRot.current += brInc;

      if (flWheel.current) flWheel.current.rotation.x = flWheelRot.current;
      if (frWheel.current) frWheel.current.rotation.x = frWheelRot.current;
      if (blWheel.current) blWheel.current.rotation.x = blWheelRot.current;
      if (brWheel.current) brWheel.current.rotation.x = brWheelRot.current;

      // Steering only on pivots
      if (flPivot.current) flPivot.current.rotation.y = fl_wheel_angle;
      if (frPivot.current) frPivot.current.rotation.y = fr_wheel_angle;

      // Car body position: average of spring extensions above wheels
      if (carBody.current) {
        // Each spring extends from wheelY upward by its spring length
        const flTop = flSpringLength;
        const frTop = frSpringLength;
        const blTop = blSpringLength;
        const brTop = brSpringLength;
        // Center position
        const avgY = (flTop + frTop + blTop + brTop) / 4;
        carBody.current.position.y = avgY;

        // Pitch (x axis): difference between front and rear average
        const frontAvg = (flTop + frTop) / 2;
        const rearAvg = (blTop + brTop) / 2;
        carBody.current.rotation.x = (rearAvg - frontAvg) * 0.3 + Math.PI / 2;

        // Roll (z axis): difference between left and right average
        const leftAvg = (flTop + blTop) / 2;
        const rightAvg = (frTop + brTop) / 2;
        carBody.current.rotation.z = (rightAvg - leftAvg) * 0.3 + Math.PI / 2;

        carBody.current.rotation.y = -Math.PI; // Face along +Z
      }
    });

    const scale = 5;

    return (
      <group>
        {/* Car body - load STL model if available (scale X and Z to 90%) */}
        <CarBodyModel ref={carBody} position={[0, 0.25, 0]} scale={scale} />

        {/* Wheels locked to ground, at corners */}
        {/* Front Left (with pivot for steering) */}
        <group ref={flPivot} position={[-0.28 * scale, 0, 0.27 * scale]}>
          <Wheel ref={flWheel} position={[0, 0, 0]} />
        </group>

        {/* Front Right */}
        <group ref={frPivot} position={[0.28 * scale, 0, 0.27 * scale]}>
          <Wheel ref={frWheel} position={[0, 0, 0]} />
        </group>

        {/* Back Left (no pivot needed) */}
        <Wheel ref={blWheel} position={[0.28 * scale, 0, -0.43 * scale]} />

        {/* Back Right */}
        <Wheel ref={brWheel} position={[-0.28 * scale, 0, -0.43 * scale]} />

        {/* Springs visually connect wheels to body */}
        {/* <Spring x={-1.8} z={2} length={flSpringLength} />
        <Spring x={1.8} z={2} length={frSpringLength} />
        <Spring x={-1.8} z={-2} length={blSpringLength} />
        <Spring x={1.8} z={-2} length={brSpringLength} /> */}
      </group>
    );
  }
}

const Wheel = React.forwardRef<
  THREE.Mesh,
  { position?: [number, number, number] }
>(({ position }, ref) => (
  <mesh ref={ref} position={position} rotation={[0, 0, Math.PI / 2]}>
    {/* Tire */}
    <cylinderGeometry args={[0.4, 0.4, 0.4, 32]} />
    <meshStandardMaterial color="black" />
    {/* Rim */}
    <mesh position={[0, 0, 0]}>
      <cylinderGeometry args={[0.3, 0.3, 0.45, 16]} />
      <meshStandardMaterial color="gray" />
    </mesh>
    {/* Central hub */}
    <mesh position={[0, 0, 0]}>
      <cylinderGeometry args={[0.2, 0.2, 0.5, 16]} />
      <meshStandardMaterial color="silver" />
    </mesh>
    {/* X rim pattern on both side faces */}
    <mesh position={[0, 0, 0.1]} rotation={[0, 0, Math.PI / 2]}>
      <boxGeometry args={[0.51, 0.06, 0.375]} />
      <meshStandardMaterial color="yellow" />
    </mesh>
    <mesh position={[0, 0, -0.1]} rotation={[0, 0, Math.PI / 2]}>
      <boxGeometry args={[0.51, 0.06, 0.375]} />
      <meshStandardMaterial color="yellow" />
    </mesh>
  </mesh>
));

Wheel.displayName = "Wheel";

function Spring({
  x,
  z,
  length,
}: {
  x: number;
  z: number;
  length: number;
  baseY?: number;
}) {
  // Spring goes from wheel (baseY) up to body (baseY + length)
  return (
    <mesh position={[x, length / 2, z]}>
      <cylinderGeometry args={[0.1, 0.1, length, 16]} />
      <meshStandardMaterial color="silver" wireframe />
    </mesh>
  );
}

function StlModel({ url, ...props }) {
  // This is a workaround for the fact that STLLoader is not available in the main three.js bundle
  // and must be imported from the examples folder.
  const { STLLoader } = require("three/examples/jsm/loaders/STLLoader");
  const geom = useLoader(STLLoader, url);
  return (
    <mesh {...props} geometry={geom}>
      <meshStandardMaterial color="gray" />
    </mesh>
  );
}

// CarBodyModel: loads `public/models/carBody.stl` and forwards a mesh ref.
const CarBodyModel = React.forwardRef<
  THREE.Mesh,
  { position?: [number, number, number]; scale?: number }
>(({ position, scale }, ref) => {
  return (
    <React.Suspense
      fallback={
        <mesh ref={ref} position={position} scale={scale}>
          <boxGeometry args={[4, 0.5, 5]} />
          <meshStandardMaterial color="orange" />
        </mesh>
      }
    >
      <StlModel
        ref={ref}
        position={position}
        scale={scale}
        url="/models/carBody.stl"
      />
    </React.Suspense>
  );
});

CarBodyModel.displayName = "CarBodyModel";
