#include "TorqueMap.h"
#include "Lookup2D.h"
#include "util.h"

#include <stdbool.h>

static const float torque_table[LOOKUP2D_POINTS_Y][LOOKUP2D_POINTS_X] = {
    // APPS = 0.0      0.5      1.0
    {0.0f,             110.0f,  220.0f}, // 0 rpm
    {0.0f,             110.0f,  220.0f}, // 2000 rpm
    {0.0f,              95.0f,  190.0f}, // 4000 rpm
    {0.0f,              62.5f,  125.0f}, // 6000 rpm
    {0.0f,              47.5f,   95.0f}, // 8000 rpm
};

static Lookup2D torque_lookup;
static bool torque_lookup_initialized = false;

static void torque_map_init_once(void) {
  if (torque_lookup_initialized) {
    return;
  }

  Lookup2D_init(&torque_lookup,
                0.0f,     // x0 = APPS min
                1.0f,     // x1 = APPS max
                0.0f,     // y0 = RPM min
                8000.0f,  // y1 = RPM max
                torque_table);

  torque_lookup_initialized = true;
}

void torque_map_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                         vcu_parameters_t *params, uint32_t dt_ms) {
  (void)params;
  (void)dt_ms;

  torque_map_init_once();

  float apps = clamp_f(out->accel_pedal_travel, 0.0f, 1.0f);
  float rpm = clamp_f(in->motor_speed_rpm, 0.0f, 8000.0f);

  out->torque_cmd = Lookup2D_evaluate(&torque_lookup, apps, rpm);
}