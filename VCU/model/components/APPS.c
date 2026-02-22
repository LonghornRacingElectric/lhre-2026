#include "APPS.h"

#include "util.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

float apps_adc_to_travel(float adc, float min, float max) {
  float raw = clamp_f(adc, min, max);
  return inverse_linear_interp(min, max, raw);
}

bool apps_out_of_range(float travel, float min, float max) {
  return travel < min || travel > max;
}

bool apps_under_voltage(float adc, float min, float max) { return adc < min; }

bool apps_over_voltage(float adc, float min, float max) { return adc > max; }

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
  out->faults.apps1_over_range =
      apps_over_voltage(in->apps1_raw, params->apps.apps1_min_adc_v,
                        params->apps.apps1_max_adc_v);
  out->faults.apps1_under_range =
      apps_under_voltage(in->apps1_raw, params->apps.apps1_min_adc_v,
                         params->apps.apps1_max_adc_v);
  out->faults.apps2_over_range =
      apps_over_voltage(in->apps2_raw, params->apps.apps2_min_adc_v,
                        params->apps.apps2_max_adc_v);
  out->faults.apps2_under_range =
      apps_under_voltage(in->apps2_raw, params->apps.apps2_min_adc_v,
                         params->apps.apps2_max_adc_v);

  out->faults.apps_any_fault =
      out->faults.apps_implaus || out->faults.apps1_over_range ||
      out->faults.apps1_under_range || out->faults.apps2_over_range ||
      out->faults.apps2_under_range;

  // Debugging fields
  out->debug.apps_implaus_ms = state->apps_implaus_ms;
  out->debug.apps_diff = out->apps1_travel - out->apps2_travel;

  // set pedal to the average of the two sensors with deadzone applied
  float raw_average_pedal =
      linear_interp(out->apps1_travel, out->apps2_travel, 0.5f);

  out->pedal =
      apply_deadzone(raw_average_pedal, params->apps.min_travel_deadzone,
                     params->apps.max_travel_deadzone);

  // apply EMA filter to compute filtered pedal
  out->pedal_filtered = ema_filter_evaluate(&state->pedal_filter, out->pedal,
                                            params->apps.pedal_ema_alpha);
}

bool apps_implausible(float travel1, float travel2, apps_state_t *state,
                      vcu_parameters_t *params, uint32_t dt_ms) {

  float max_travel = fmaxf(travel1, travel2);

  // if our APPS sensors have a diff > than our threshold, we count until they
  // are implausible
  if ((max_travel > params->apps.min_travel_threshold) &&
      fabsf(travel1 - travel2) > params->apps.max_allowable_diff) {
    state->apps_implaus_ms += dt_ms;
    if (state->apps_implaus_ms > params->apps.implaus_debounce_time_ms) {
      state->apps_implaus = true;
    }
  } else {
    // reset counter at any point
    state->apps_implaus_ms = 0;
  }

  // if the APPS pedal is IDLE, we can reset implausibility
  if (state->apps_implaus &&
      max_travel <= params->apps.max_travel_restore_threshold) {
    state->apps_implaus = false;
    state->apps_implaus_ms = 0;
  }

  return state->apps_implaus;
}
