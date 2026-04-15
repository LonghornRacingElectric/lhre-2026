#include "TorqueMap.h"
#include "Lookup2D.h"
#include "util.h"

#include <stdbool.h>
#include <math.h>

#define MAX_MOTOR_RPM         6000.0f
#define BATTERY_POWER_W       80000.0f
#define EFFICIENCY 0.90f
#define PI_F                  3.14159265f

static float torque_table[LOOKUP2D_POINTS_Y][LOOKUP2D_POINTS_X];
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

static void build_torque_table(float max_torque_nm) {
  for (int row = 0; row < LOOKUP2D_POINTS_Y; row++) {
    float rpm = ((float)row / (float)(LOOKUP2D_POINTS_Y - 1)) * MAX_MOTOR_RPM;
    float max_torque = calc_max_torque_at_rpm(rpm, max_torque_nm);

    torque_table[row][0] = 0.0f;
    torque_table[row][1] = 0.5f * max_torque;
    torque_table[row][2] = max_torque;
  }
}

static void torque_map_init(float max_torque_nm) {
  build_torque_table(max_torque_nm);

  Lookup2D_init(&torque_lookup,
                0.0f,          // x0 = APPS min
                1.0f,          // x1 = APPS max
                0.0f,          // y0 = RPM min
                MAX_MOTOR_RPM, // y1 = RPM max
                torque_table);
}

void torque_map_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                         vcu_parameters_t *params, uint32_t dt_ms) {
  (void)dt_ms;

  float max_torque_nm = params->torque_map.max_torque_nm;
  if (max_torque_nm < 0.0f) {
    max_torque_nm = 0.0f;
  }

  torque_map_init(max_torque_nm);

  float apps = clamp_f(out->accel_pedal_travel, 0.0f, 1.0f);
  float rpm = clamp_f(in->motor_speed_rpm, 0.0f, MAX_MOTOR_RPM);

  out->torque_cmd = Lookup2D_evaluate(&torque_lookup, apps, rpm);
}
