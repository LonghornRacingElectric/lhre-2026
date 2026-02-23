#include "BSE.h"
#include "util.h"
#include <math.h>
#include <vcu_parameters.h>

/**
 * @brief Converts raw ADC value to brake pressure in psi
 */
float bse_adc_to_psi(float adc, float min_adc, float max_adc, float max_psi) {
  float adc_clamped = clamp_f(adc, min_adc, max_adc);
  float pct = inverse_linear_interp(min_adc, max_adc, adc_clamped);
  return linear_interp(0.0f, max_psi, pct);
}

bool bse_under_voltage(float adc, float min, float max) { return adc < min; }

bool bse_over_voltage(float adc, float min, float max) { return adc > max; }

void bse_init(bse_state_t *state) {
  state->brake_active = false;
  state->brake_latched = false;
  ema_filter_init(&state->bse_filter);
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
  out->bse1_psi = bse_adc_to_psi(
      in->bse1_raw, params->bse.bse1_adc_at_min_psi_v,
      params->bse.bse1_adc_at_max_psi_v, params->bse.bse_max_psi);
  out->bse2_psi = bse_adc_to_psi(
      in->bse2_raw, params->bse.bse2_adc_at_min_psi_v,
      params->bse.bse2_adc_at_max_psi_v, params->bse.bse_max_psi);

  out->faults.bse1_under_range =
      bse_under_voltage(in->bse1_raw, params->bse.bse1_adc_at_min_psi_v,
                        params->bse.bse1_adc_at_max_psi_v);
  out->faults.bse1_over_range =
      bse_over_voltage(in->bse1_raw, params->bse.bse1_adc_at_min_psi_v,
                       params->bse.bse1_adc_at_max_psi_v);
  out->faults.bse2_under_range =
      bse_under_voltage(in->bse2_raw, params->bse.bse2_adc_at_min_psi_v,
                        params->bse.bse2_adc_at_max_psi_v);
  out->faults.bse2_over_range =
      bse_over_voltage(in->bse2_raw, params->bse.bse2_adc_at_min_psi_v,
                       params->bse.bse2_adc_at_max_psi_v);

  out->faults.brake_any_fault =
      out->faults.bse1_under_range || out->faults.bse1_over_range ||
      out->faults.bse2_under_range || out->faults.bse2_over_range;

  float raw_average_psi = linear_interp(out->bse1_psi, out->bse2_psi, 0.5f);

  // apply_deadzone maps to 0-1, so scale up to max_psi
  out->bse_psi = apply_deadzone(raw_average_psi, params->bse.min_psi_deadzone,
                                params->bse.max_psi_deadzone) *
                 params->bse.bse_max_psi;

  out->bse_psi_filtered = ema_filter_evaluate(&state->bse_filter, out->bse_psi,
                                              params->bse.bse_ema_alpha);

  out->brake_active = bse_is_active(out->bse_psi_filtered, state, params);

  out->faults.brake_latched =
      bse_is_latched(out->brake_active, out->pedal_filtered, state, params);

  out->faults.brake_any_fault =
      out->faults.brake_any_fault || out->faults.brake_latched;
}
