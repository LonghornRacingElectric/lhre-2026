#include "BSE.h"
#include "util.h"
#include <vcu_parameters.h>

/**
 * @brief Converts raw ADC value to brake pressure in psi
 */
float bse_adc_to_psi(uint16_t adc, vcu_parameters_t *params) {
  uint16_t adc_clamped = clamp_u16(adc, params->bse.bse_adc_at_min_psi_v,
                                   params->bse.bse_adc_at_max_psi_v);

  float pct =
      inverse_linear_interp(params->bse.bse_adc_at_min_psi_v,
                            params->bse.bse_adc_at_max_psi_v, adc_clamped);

  return linear_interp(0.0f, params->bse.bse_max_psi, pct);
}

void bse_init(bse_state_t *state) {
  state->brake_active = false;
  state->brake_latched = false;
}

/**
 * @brief Checks if brake is active based on pressure threshold
 */
bool bse_is_active(float psi, bse_state_t *state, vcu_parameters_t *params) {
  // lagging hysteresis
  if (state->brake_active && psi < params->bse.bse_off_psi) {
    state->brake_active = false;
  } else if (!state->brake_active && psi >= params->bse.bse_on_psi) {
    state->brake_active = true;
  }

  return state->brake_active;
}

static bool bse_is_latched(bool brake_active, float pedal, bse_state_t *state,
                           vcu_parameters_t *params) {
  if (brake_active && pedal > params->bse.max_pedal_while_braking) {
    state->brake_latched = true;
  }

  // can only be unlatched if we drop below the threshold
  if (state->brake_latched && pedal < params->bse.max_pedal_restore_threshold) {
    state->brake_latched = false;
  }

  return state->brake_latched;
}

void bse_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                  bse_state_t *state, vcu_parameters_t *params,
                  uint32_t dt_ms) {
  out->bse_psi = bse_adc_to_psi(in->bse_raw, params);

  out->brake_active = bse_is_active(out->bse_psi, state, params);

  out->faults.brake_latched =
      bse_is_latched(out->brake_active, out->pedal_filtered, state, params);

  out->faults.brake_any_fault = out->faults.brake_latched;
}
