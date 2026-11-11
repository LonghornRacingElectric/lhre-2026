#include "PowerLimit.h"
#include "TorqueMap.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* Exercise the actual TorqueMap -> PowerLimit boundary without MCU hardware. */
static unsigned failures;

static void expect_near(const char *name, float actual, float expected) {
  if (!isfinite(actual) || fabsf(actual - expected) > 0.0001f) {
    fprintf(stderr, "%s: expected %.4f Nm, got %.4f Nm\n", name, expected,
            actual);
    ++failures;
  }
}

static vcu_parameters_t make_parameters(void) {
  vcu_parameters_t params = {0};
  for (unsigned i = 0; i < LOOKUP1D_POINTS; ++i) {
    params.torque_map.power_limit_torque[i] = 100.0f;
    params.torque_map.pedal_map[i] = (float)i / (LOOKUP1D_POINTS - 1);
  }
  params.torque_map.pedal_curve_exponent = 1.0f;
  params.torque_map.low_cell_cutoff_v = 3.0f;
  params.torque_map.low_cell_derate_start_v = 3.5f;
  params.power_limit.power_limit_w = 1000.0f;
  params.power_limit.power_limit_trim_kp = 0.1f;
  params.power_limit.power_limit_trim_integral_max = 1000.0f;
  return params;
}

int main(void) {
  static const struct {
    const char *name;
    float cell_voltage;
    float battery_current;
    float expected_derated;
    float expected_limited;
  } cases[] = {
      {"zero derate factor preserves cutoff", 3.0f, 5.0f, 0.0f, 0.0f},
      {"partial derate without power trim", 3.25f, 5.0f, 50.0f, 50.0f},
      {"partial derate plus power trim", 3.25f, 12.0f, 50.0f, 30.0f},
      {"full request without derating", 3.5f, 5.0f, 100.0f, 100.0f},
      {"power trim without derating", 3.5f, 12.0f, 100.0f, 80.0f},
      {"power trim saturates at remaining torque", 3.25f, 20.0f, 50.0f, 0.0f},
  };

  vcu_parameters_t params = make_parameters();
  torque_map_init(&params);
  for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    vcu_inputs_t in = {0};
    vcu_outputs_t out = {0};
    power_limit_state_t state;
    power_limit_init(&state, &params);
    in.battery_voltage_v = 100.0f;
    in.battery_current_a = cases[i].battery_current;
    out.accel_pedal_travel = 1.0f;
    out.open_circuit_cell_voltage = cases[i].cell_voltage;

    torque_map_evaluate(&in, &out, &params, 10);
    expect_near("full-pedal lookup fixture", out.torque_lookup_output, 100.0f);
    expect_near("upstream derate fixture", out.torque_derated,
                cases[i].expected_derated);
    power_limit_evaluate(&in, &out, &state, &params, 10);
    expect_near(cases[i].name, out.torque_power_limited,
                cases[i].expected_limited);
  }

  /* A release must stay at zero even with a nonzero power-limit integrator. */
  vcu_inputs_t in = {0};
  vcu_outputs_t out = {0};
  power_limit_state_t state;
  power_limit_init(&state, &params);
  params.power_limit.power_limit_trim_kp = 0.0f;
  params.power_limit.power_limit_trim_ki = 0.1f;
  in.battery_voltage_v = 100.0f;
  in.battery_current_a = 20.0f;
  out.accel_pedal_travel = 1.0f;
  out.open_circuit_cell_voltage = 3.5f;
  torque_map_evaluate(&in, &out, &params, 10);
  power_limit_evaluate(&in, &out, &state, &params, 10);
  expect_near("integral trim before release", out.torque_power_limited, 99.0f);
  out.accel_pedal_travel = 0.0f;
  torque_map_evaluate(&in, &out, &params, 10);
  power_limit_evaluate(&in, &out, &state, &params, 10);
  expect_near("driver release with stored integral", out.torque_power_limited,
              0.0f);

  if (failures != 0) {
    fprintf(stderr, "%u power-limit regression assertion(s) failed\n", failures);
    return EXIT_FAILURE;
  }
  puts("Power-limit regression tests passed (6 cases plus driver release).");
  return EXIT_SUCCESS;
}
