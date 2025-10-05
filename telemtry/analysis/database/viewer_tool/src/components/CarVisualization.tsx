"use client";

import { Canvas, useFrame } from "@react-three/fiber";
import { OrbitControls } from "@react-three/drei";
import { useRef } from "react";
import * as THREE from "three";
import React from "react";

const CarConstants = {
  suspension: {
    rest_length: 0.7,
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
  wheel_speed: number;
  fl_wheel_angle: number;
  fr_wheel_angle: number;
  fl_spring_displace: number;
  fr_spring_displace: number;
  bl_spring_displace: number;
  br_spring_displace: number;
};
export default function CarVisualization() {
  const [carState, setCarState] = React.useState<CarState>({
    wheel_speed: 2,
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
      <div className="flex gap-4 mb-2 overflow-x-auto">
        {/* ...existing code for controls... */}
        <label>
          Wheel Speed
          <input
            type="range"
            min={CarConstants.wheel.min_speed}
            max={CarConstants.wheel.max_speed}
            step={0.1}
            value={carState.wheel_speed}
            onChange={(e) =>
              setCarState((s) => ({
                ...s,
                wheel_speed: Number(e.target.value),
              }))
            }
          />
          {carState.wheel_speed}
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
      <div className="flex-grow">
        <Canvas camera={{ position: [0, 5, 12], fov: 50 }}>
          <ambientLight intensity={0.5} />
          <directionalLight position={[5, 10, 5]} intensity={1} />
          <SuspensionSystem
            {...carState}
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
    wheel_speed,
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
      const rotationSpeed = -wheel_speed * delta;

      // Roll wheels (local X axis), persist rotation
      flWheelRot.current -= rotationSpeed;
      frWheelRot.current -= rotationSpeed;
      blWheelRot.current -= rotationSpeed;
      brWheelRot.current -= rotationSpeed;

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
        const avgY = (flTop + frTop + blTop + brTop) / 4 + 0.25;
        carBody.current.position.y = avgY;

        // Pitch (x axis): difference between front and rear average
        const frontAvg = (flTop + frTop) / 2;
        const rearAvg = (blTop + brTop) / 2;
        carBody.current.rotation.x = (rearAvg - frontAvg) * 0.3;

        // Roll (z axis): difference between left and right average
        const leftAvg = (flTop + blTop) / 2;
        const rightAvg = (frTop + brTop) / 2;
        carBody.current.rotation.z = (rightAvg - leftAvg) * 0.3;
      }
    });

    return (
      <group>
        {/* Car body */}
        <mesh ref={carBody} position={[0, 0.5, 0]}>
          <boxGeometry args={[4, .5, 5]} />
          <meshStandardMaterial color="orange" />
        </mesh>

        {/* Wheels locked to ground, at corners */}
        {/* Front Left (with pivot for steering) */}
        <group ref={flPivot} position={[-1.8, 0, 2]}>
          <Wheel ref={flWheel} position={[0, 0, 0]} />
        </group>

        {/* Front Right */}
        <group ref={frPivot} position={[1.8, 0, 2]}>
          <Wheel ref={frWheel} position={[0, 0, 0]} />
        </group>

        {/* Back Left (no pivot needed) */}
        <Wheel ref={blWheel} position={[-1.8, 0, -2]} />

        {/* Back Right */}
        <Wheel ref={brWheel} position={[1.8, 0, -2]} />

        {/* Springs visually connect wheels to body */}
        <Spring x={-1.8} z={2} length={flSpringLength} />
        <Spring x={1.8} z={2} length={frSpringLength} />
        <Spring x={-1.8} z={-2} length={blSpringLength} />
        <Spring x={1.8} z={-2} length={brSpringLength} />
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
    <cylinderGeometry args={[0.6, 0.6, 0.4, 32]} />
    <meshStandardMaterial color="black" />
    {/* Rim */}
    <mesh position={[0, 0, 0]}>
      <cylinderGeometry args={[0.4, 0.4, 0.45, 16]} />
      <meshStandardMaterial color="gray" />
    </mesh>
    {/* Central hub */}
    <mesh position={[0, 0, 0]}>
      <cylinderGeometry args={[0.2, 0.2, 0.5, 16]} />
      <meshStandardMaterial color="silver" />
    </mesh>
    {/* X rim pattern on both side faces */}
    {/* Left side face */}
    <mesh position={[0, 0, 0.2]} rotation={[0, 0, 0]}>
      <boxGeometry args={[0.45, 0.08, 0.4]} />
      <meshStandardMaterial color="yellow" />
    </mesh>
    <mesh position={[0, 0, 0.2]} rotation={[0, 0, Math.PI / 2]}>
      <boxGeometry args={[0.45, 0.08, 0.4]} />
      <meshStandardMaterial color="yellow" />
    </mesh>
    {/* Right side face */}
    <mesh position={[0, 0, -0.2]} rotation={[0, 0, 0]}>
      <boxGeometry args={[0.45, 0.08, 0.4]} />
      <meshStandardMaterial color="yellow" />
    </mesh>
    <mesh position={[0, 0, -0.2]} rotation={[0, 0, Math.PI / 2]}>
      <boxGeometry args={[0.45, 0.08, 0.4]} />
      <meshStandardMaterial color="yellow" />
    </mesh>
    {/* Multiple spokes for better rotation visibility */}
    {[...Array(4)].map((_, i) => (
      <mesh key={i} rotation={[0, 0, (Math.PI / 2) * i]}>
        <boxGeometry args={[0.05, 0.05, 0.9]} />
        <meshStandardMaterial color={i % 2 === 0 ? "red" : "white"} />
      </mesh>
    ))}
  </mesh>
));

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
