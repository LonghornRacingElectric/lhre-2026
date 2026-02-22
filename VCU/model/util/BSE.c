#include "BSE.h"
#include "util.h"

/**
 * @brief Converts raw ADC value to brake pressure in psi
 */
float bse_adc_to_psi(uint16_t adc, vcu_parameters_t *params) {
  uint16_t adc_clamped = clamp_u16(adc, params->bse.bse_adc_at_min_psi,
                                   params->bse.bse_adc_at_max_psi);

  return linear_interp(0.0f, params->bse.bse_max_psi,
                       (float)adc_clamped /
                           (float)params->bse.bse_adc_at_max_psi);
}

/**
 * @brief Checks if brake is active based on pressure threshold
 */
bool bse_is_active(float psi, vcu_parameters_t *params) {
  static bool brake_active = false;

  // lagging hysteresis
  if (brake_active && psi < params->bse.bse_off_psi) {
    brake_active = false;
  } else if (!brake_active && psi >= params->bse.bse_on_psi) {
    brake_active = true;
  }

  return brake_active;
}

static bool bse_is_latched(float psi, bool brake_active, float pedal,
                           vcu_parameters_t *params) {
  static bool brake_latched = false;

  if (brake_active && pedal > params->bse.max_pedal_while_braking) {
    brake_latched = true;
  }

  // can only be unlatched if we drop below the threshold
  if (brake_latched && pedal < params->bse.max_pedal_restore_threshold) {
    brake_latched = false;
  }

  return brake_latched;
}

void bse_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                  vcu_parameters_t *params, uint32_t dt_ms) {
  out->bse_psi = bse_adc_to_psi(in->bse_raw, params);

  out->brake_active = bse_is_active(out->bse_psi, params);

  out->brake_latched = bse_is_latched(out->bse_psi, out->brake_active,
                                      out->pedal_filtered, params);
}
