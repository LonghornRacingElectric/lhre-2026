// Port of channels.py — Orion channel registry mapping DB tables/columns to MoTeC channels.

import type { ChannelDef } from "./types";

export const ORION_CHANNELS: ChannelDef[] = [
  { key: "motor_rpm", label: "Motor RPM", table: "controls", column: "motor_speed", unit: "rpm", quantity: "speed", default: true, split_candidate: true },
  { key: "rpm_request", label: "RPM Request", table: "controls", column: "rpm_request", unit: "rpm", quantity: "speed", default: false, split_candidate: true },
  { key: "wheel_speed", label: "Wheel Speed", table: "dynamics", column: "wheel_speed", unit: "rad/s", quantity: "speed", default: false, split_candidate: true },
  { key: "gps_speed", label: "GPS Speed", table: "dynamics", column: "gps_speed", unit: "m/s", quantity: "speed", default: false, split_candidate: true },
  { key: "gps_latitude", label: "GPS Latitude", table: "dynamics", column: "gps[1]", unit: "deg", quantity: "position", default: false, split_candidate: false },
  { key: "gps_longitude", label: "GPS Longitude", table: "dynamics", column: "gps[2]", unit: "deg", quantity: "position", default: false, split_candidate: false },
  { key: "apps1_travel", label: "APPS 1 Travel", table: "controls", column: "apps1_travel", unit: "%", quantity: "position", default: false, split_candidate: false },
  { key: "apps2_travel", label: "APPS 2 Travel", table: "controls", column: "apps2_travel", unit: "%", quantity: "position", default: false, split_candidate: false },
  { key: "brake_pressure_f", label: "Brake Pressure Front", table: "controls", column: "brake_pressure_f", unit: "psi", quantity: "pressure", default: false, split_candidate: false },
  { key: "torque_request", label: "Torque Request", table: "controls", column: "torque_request", unit: "Nm", quantity: "torque", default: false, split_candidate: false },
  { key: "torque_feedback", label: "Torque Feedback", table: "controls", column: "torque_feedback", unit: "Nm", quantity: "torque", default: false, split_candidate: false },
  { key: "commanded_torque", label: "Commanded Torque", table: "controls", column: "commanded_torque", unit: "Nm", quantity: "torque", default: false, split_candidate: false },
  { key: "steer_col_angle", label: "Steering Column Angle", table: "dynamics", column: "steer_col_angle", unit: "deg", quantity: "angle", default: false, split_candidate: false },
  { key: "flw_speed", label: "Front Left Wheel Speed", table: "dynamics", column: "flw_speed", unit: "rad/s", quantity: "speed", default: false, split_candidate: false },
  { key: "frw_speed", label: "Front Right Wheel Speed", table: "dynamics", column: "frw_speed", unit: "rad/s", quantity: "speed", default: false, split_candidate: false },
  { key: "blw_speed", label: "Back Left Wheel Speed", table: "dynamics", column: "blw_speed", unit: "rad/s", quantity: "speed", default: false, split_candidate: false },
  { key: "brw_speed", label: "Back Right Wheel Speed", table: "dynamics", column: "brw_speed", unit: "rad/s", quantity: "speed", default: false, split_candidate: false },
  { key: "hv_pack_v", label: "HV Pack Voltage", table: "pack", column: "hv_pack_v", unit: "V", quantity: "voltage", default: false, split_candidate: false },
  { key: "hv_c", label: "HV Current", table: "pack", column: "hv_c", unit: "A", quantity: "current", default: false, split_candidate: false },
  { key: "hv_soc", label: "HV State of Charge", table: "pack", column: "hv_soc", unit: "%", quantity: "ratio", default: false, split_candidate: false },
  { key: "dc_bus_v", label: "DC Bus Voltage", table: "pack", column: "dc_bus_v", unit: "V", quantity: "voltage", default: false, split_candidate: false },
  { key: "dc_bus_current", label: "DC Bus Current", table: "pack", column: "dc_bus_current", unit: "A", quantity: "current", default: false, split_candidate: false },
  { key: "motor_temp", label: "Motor Temp", table: "thermal", column: "motor_temp", unit: "C", quantity: "temperature", default: false, split_candidate: false },
  { key: "inverter_temp", label: "Inverter Temp", table: "thermal", column: "inverter_temp", unit: "C", quantity: "temperature", default: false, split_candidate: false },
  { key: "coolant_temp", label: "Coolant Temp", table: "thermal", column: "coolant_temp", unit: "C", quantity: "temperature", default: false, split_candidate: false },
  { key: "ambient_temp", label: "Ambient Temp", table: "thermal", column: "ambient_temp", unit: "C", quantity: "temperature", default: false, split_candidate: false },
];

export const DEFAULT_CHANNEL_KEY = ORION_CHANNELS.find((c) => c.default)!.key;

export function getChannel(channels: ChannelDef[], key: string | null | undefined): ChannelDef {
  const lookup = new Map(channels.map((c) => [c.key, c]));
  const found = lookup.get(key || DEFAULT_CHANNEL_KEY);
  if (!found) throw new Error(`Unknown telemetry channel: ${key}`);
  return found;
}
