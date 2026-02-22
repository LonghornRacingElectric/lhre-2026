#ifndef BSE_H
#define BSE_H

#include "vcu_inputs.h"
#include "vcu_outputs.h"
#include "vcu_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Converts raw ADC value to brake pressure in psi
 */
float bse_adc_to_psi(uint16_t adc, vcu_parameters_t *params);

/**
 * @brief Checks if brake is active based on pressure threshold
 */
bool bse_is_active(float psi, vcu_parameters_t *params);

/**
 * @brief Steps through the BSE logic and updates outputs
 */
void bse_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                  vcu_parameters_t *params, uint32_t dt_ms);

#ifdef __cplusplus
}
#endif

#endif // BSE_H