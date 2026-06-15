#include "TorqueMap.h"
#include "Lookup2D.h"
#include "util.h"

#include <stdbool.h>
#include <math.h>

#define MAX_MOTOR_RPM         6000.0f
#define BATTERY_POWER_W       80000.0f
#define EFFICIENCY 0.96f
#define PI_F                  3.14159265f

static Lookup1D torque_lookup;
static Lookup1D pedal_lookup;

static float calc_max_torque_at_rpm(float rpm, float max_torque_nm) {
  const float mech_power_w = BATTERY_POWER_W * EFFICIENCY;

  if (rpm <= 1.0f) {
    return max_torque_nm;
  }

  float omega = rpm * 2.0f * PI_F / 60.0f;
  float torque = mech_power_w / omega;

  if (torque > max_torque_nm) {
    torque = max_torque_nm;
  }
  if (torque < 0.0f) {
    torque = 0.0f;
  }

  return torque;
}

void torque_map_init(const vcu_parameters_t *params) {
  Lookup1D_init(&torque_lookup,
                0.0f,
                MAX_MOTOR_RPM,
                params->torque_map.power_limit_torque);
  Lookup1D_init(&pedal_lookup,
                0.0f,
                1.0f,
                params->torque_map.pedal_map);
}

static float compute_low_cell_voltage_derate(float min_cell_voltage,
                                             const vcu_parameters_t *params) {
  const float derate_start_v = params->torque_map.low_cell_derate_start_v;
  const float cutoff_v = params->torque_map.low_cell_cutoff_v;

  if (derate_start_v <= cutoff_v) {
    return 1.0f;
  }

  if (min_cell_voltage >= derate_start_v) {
    return 1.0f;
  }

  if (min_cell_voltage <= cutoff_v) {
    return 0.0f;
  }

  return (min_cell_voltage - cutoff_v) / (derate_start_v - cutoff_v);
}

void torque_map_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                         const vcu_parameters_t *params, uint32_t dt_ms) {

  float apps = clamp_f(out->accel_pedal_travel, 0.0f, 1.0f);
  float rpm = clamp_f(in->motor_speed_rpm, 0.0f, MAX_MOTOR_RPM);

  float available_torque = Lookup1D_evaluate(&torque_lookup, rpm);
  float pedal_remapped = Lookup1D_evaluate(&pedal_lookup, apps);
  float pedal_percent_of_available_torque = powf(pedal_remapped, params->torque_map.pedal_curve_exponent);
  
  out->torque_lookup_output = available_torque * pedal_percent_of_available_torque;

  out->derate_factor_cell_voltage = compute_low_cell_voltage_derate(out->open_circuit_cell_voltage, params);
  out->derate_factor_cell_temp = 1.0f; // TODO: cell temp derate

  // TODO - torque_derated isn't even used, whoops. So derates don't do anything
  out->torque_derated = out->torque_lookup_output
  * out->derate_factor_cell_voltage
  * out->derate_factor_cell_temp;
}
