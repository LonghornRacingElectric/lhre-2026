#include "TorqueMap.h"
#include "util.h"

static const float apps_bp[] = {0.0f, 0.5f, 1.0f};
static const float rpm_bp[]  = {0.0f, 2000.0f, 4000.0f, 6000.0f, 8000.0f};

static const float torque_table[5][3] = {
    {0.0f, 110.0f, 220.0f},
    {0.0f, 110.0f, 220.0f},
    {0.0f,  95.0f, 190.0f},
    {0.0f,  62.5f, 125.0f},
    {0.0f,  47.5f,  95.0f},
};

void torque_map_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                         vcu_parameters_t *params, uint32_t dt_ms) {
  (void)params;
  (void)dt_ms;

  float apps = clamp_f(out->accel_pedal_travel, 0.0f, 1.0f);
  float rpm = clamp_f(in->motor_speed_rpm, rpm_bp[0], rpm_bp[3]);

  int i = 0;
  while (i < 2 && rpm > rpm_bp[i + 1]) i++;

  int j = 0;
  while (j < 1 && apps > apps_bp[j + 1]) j++;

  float rpm_pct = inverse_linear_interp(rpm_bp[i], rpm_bp[i + 1], rpm);
  float apps_pct = inverse_linear_interp(apps_bp[j], apps_bp[j + 1], apps);

  float t00 = torque_table[i][j];
  float t01 = torque_table[i][j + 1];
  float t10 = torque_table[i + 1][j];
  float t11 = torque_table[i + 1][j + 1];

  float t0 = linear_interp(t00, t01, apps_pct);
  float t1 = linear_interp(t10, t11, apps_pct);

  out->torque_cmd = linear_interp(t0, t1, rpm_pct);
}