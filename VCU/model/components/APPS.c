#include "APPS.h"

#include "util.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

float apps_adc_to_travel(uint16_t adc, uint16_t min, uint16_t max) {
  uint16_t raw = clamp_u16(adc, min, max);
  return inverse_linear_interp(min, max, raw);
}

void apps_init(apps_state_t *state) {
  state->apps_implaus = false;
  state->apps_implaus_ms = 0;
}

void apps_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                   apps_state_t *state, vcu_parameters_t *params,
                   uint32_t dt_ms) {
  out->apps1_travel = apps_adc_to_travel(
      in->apps1_raw, params->apps.apps1_min_adc, params->apps.apps1_max_adc);
  out->apps2_travel = apps_adc_to_travel(
      in->apps2_raw, params->apps.apps2_min_adc, params->apps.apps2_max_adc);

  out->apps_implaus = apps_implausible(out->apps1_travel, out->apps2_travel,
                                       state, params, dt_ms);
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
