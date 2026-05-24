#include "TorqueMap.h"
#include "Lookup2D.h"
#include "util.h"

#include <stdbool.h>
#include <math.h>

#define MAX_MOTOR_RPM         6000.0f
#define BATTERY_POWER_W       80000.0f
#define EFFICIENCY 0.96f
#define PI_F                  3.14159265f

// X axis: APPS (0.0 to 1.0), mapped to torque demand 0–220 Nm in 11 equal steps
// Y axis: motor RPM (0 to 6000) in 11 equal steps of 600
// Values = min(power-limit torque at that RPM, linear APPS torque demand)
// Power budget: 80000 W * 0.96 efficiency = 76800 W mechanical
static const float torque_table[LOOKUP2D_POINTS_Y][LOOKUP2D_POINTS_X] = {
  /* rpm=    0 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 100.0f, 110.0f, 120.0f, 130.0f,  140.0f,  150.00f },
  /* rpm=  600 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 110.0f, 120.0f, 130.0f, 150.0f,  170.0f,  190.00f },
  /* rpm= 1200 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 110.0f, 132.0f, 154.0f, 176.0f,  198.0f,  220.00f },
  /* rpm= 1800 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 110.0f, 132.0f, 154.0f, 176.0f,  198.0f,  220.00f },
  /* rpm= 2400 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 110.0f, 132.0f, 154.0f, 176.0f,  198.0f,  220.00f },
  /* rpm= 3000 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 110.0f, 132.0f, 154.0f, 176.0f,  198.0f,  220.00f },
  /* rpm= 3600 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 110.0f, 132.0f, 154.0f, 176.0f,  198.0f,  203.72f },
  /* rpm= 4200 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 110.0f, 132.0f, 154.0f, 174.62f, 174.62f, 174.62f },
  /* rpm= 4800 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 110.0f, 132.0f, 152.79f, 152.79f, 152.79f, 152.79f },
  /* rpm= 5400 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 110.0f, 132.0f, 135.81f, 135.81f, 135.81f, 135.81f },
  /* rpm= 6000 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 110.0f, 122.23f, 122.23f, 122.23f, 122.23f, 122.23f },
};
static Lookup2D torque_lookup;

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
  (void)params;

  Lookup2D_init(&torque_lookup,
                0.0f,          // x0 = APPS min
                1.0f,          // x1 = APPS max
                0.0f,          // y0 = RPM min
                MAX_MOTOR_RPM, // y1 = RPM max
                torque_table);
}

static float compute_low_cell_voltage_derate(const vcu_inputs_t *in,
                                             const vcu_parameters_t *params) {
  const float derate_start_v = params->torque_map.low_cell_derate_start_v;
  const float cutoff_v = params->torque_map.low_cell_cutoff_v;

  if (derate_start_v <= cutoff_v) {
    return 1.0f;
  }

  if (in->min_cell_voltage_v >= derate_start_v) {
    return 1.0f;
  }

  if (in->min_cell_voltage_v <= cutoff_v) {
    return 0.0f;
  }

  return (in->min_cell_voltage_v - cutoff_v) / (derate_start_v - cutoff_v);
}

void torque_map_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                         const vcu_parameters_t *params, uint32_t dt_ms) {
  (void)dt_ms;

  float max_torque_nm = params->torque_map.max_torque_nm;
  if (max_torque_nm < 0.0f) {
    max_torque_nm = 0.0f;
  }

  float apps = clamp_f(out->accel_pedal_travel, 0.0f, 1.0f);
  float rpm = clamp_f(in->motor_speed_rpm, 0.0f, MAX_MOTOR_RPM);

  out->torque_lookup_output = Lookup2D_evaluate(&torque_lookup, apps, rpm);
  // out->torque_lookup_output = apps * 220.0f;

  out->derate_factor_cell_voltage = compute_low_cell_voltage_derate(in, params);
  out->derate_factor_cell_temp = 1.0f; // TODO: cell temp derate

  out->torque_derated = out->torque_lookup_output
  * out->derate_factor_cell_voltage
  * out->derate_factor_cell_temp;
}
