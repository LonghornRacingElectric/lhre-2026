#include "APPS.h"

#include "util.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

float apps_adc_to_travel(float adc, float min, float max) {
  return inverse_linear_interp(min, max, adc);
}

bool apps_out_of_range(float travel, float min, float max) {
  return travel < min || travel > max;
}

void apps_init(apps_state_t *state) {
  state->apps_implaus = false;
  state->apps_implaus_ms = 0;
  ema_filter_init(&state->pedal_filter);
}

void apps_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                   apps_state_t *state, vcu_parameters_t *params,
                   uint32_t dt_ms) {
  out->apps1_travel =
      apps_adc_to_travel(in->apps1_raw, params->apps.apps1_min_adc_v,
                         params->apps.apps1_max_adc_v);
  out->apps2_travel =
      apps_adc_to_travel(in->apps2_raw, params->apps.apps2_min_adc_v,
                         params->apps.apps2_max_adc_v);

  // Check faults and update outputs
  out->faults.apps_implaus = apps_implausible(
      out->apps1_travel, out->apps2_travel, state, params, dt_ms);
  out->faults.apps1_over_range = out->apps1_travel > 1.0f;
  out->faults.apps1_under_range = out->apps1_travel < -0.5f;  // TODO alice fix this bs
  out->faults.apps2_over_range = out->apps2_travel > 1.0f;
  out->faults.apps2_under_range = out->apps2_travel < -0.5f;

  out->faults.apps_any_fault =
      out->faults.apps_implaus || out->faults.apps1_over_range ||
      out->faults.apps1_under_range || out->faults.apps2_over_range ||
      out->faults.apps2_under_range;

  // Debugging fields
  out->debug.apps_implaus_ms = state->apps_implaus_ms;
  out->debug.apps_diff = out->apps1_travel - out->apps2_travel;
  out->debug.apps1_travel = out->apps1_travel;
  out->debug.apps2_travel = out->apps2_travel;

  // set pedal to the average of the two sensors
  float raw_average_pedal_travel =
      linear_interp(out->apps1_travel, out->apps2_travel, 0.5f);

  float filtered_pedal_travel =
      ema_filter_evaluate(&state->pedal_filter, raw_average_pedal_travel,
                          params->apps.pedal_ema_alpha);

  out->accel_pedal_travel =
      apply_deadzone(filtered_pedal_travel, params->apps.min_travel_deadzone,
                     params->apps.max_travel_deadzone);
}

bool apps_implausible(float travel1, float travel2, apps_state_t *state,
                      vcu_parameters_t *params, uint32_t dt_ms) {

  // if our APPS sensors have a diff > than our threshold, we start counting
  if (fabsf(travel1 - travel2) > params->apps.max_allowable_diff) {
    state->apps_implaus_ms += dt_ms;
    if (state->apps_implaus_ms > params->apps.implaus_debounce_time_ms) {
      state->apps_implaus = true;
    }
  } else {
    // reset counter at any point
    state->apps_implaus_ms = 0;
    state->apps_implaus = false;
  }

  return state->apps_implaus;
}
