#ifndef BSE_H
#define BSE_H

#include "low_pass.h"
#include "vcu_inputs.h"
#include "vcu_outputs.h"
#include "vcu_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  bool brake_pressed;
  bool brake_latched;
  ema_filter_t bse_filter;
} bse_state_t;

void bse_init(bse_state_t *state);

/**
 * @brief Converts raw ADC value to brake pressure in psi
 */
float bse_adc_to_psi(float adc, float min_adc, float max_adc, float max_psi);

/**
 * @brief Checks if brake is active based on pressure threshold
 */
bool bse_is_active(float psi, bse_state_t *state, vcu_parameters_t *params);

/**
 * @brief Steps through the BSE logic and updates outputs
 */
void bse_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                  bse_state_t *state, vcu_parameters_t *params, uint32_t dt_ms);

#ifdef __cplusplus
}
#endif

#endif // BSE_H