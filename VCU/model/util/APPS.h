#ifndef APPS_H
#define APPS_H

#include "vcu_inputs.h"
#include "vcu_outputs.h"
#include "vcu_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  bool apps_implaus;
  uint32_t apps_implaus_ms;
} apps_state_t;

float apps_adc_to_travel(uint16_t adc, uint16_t min, uint16_t max);

bool apps_implausible(float travel1, float travel2, apps_state_t *state,
                      vcu_parameters_t *params, uint32_t dt_ms);

/**
 * @brief Steps through the APPS logic and updates outputs
 */
void apps_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                   apps_state_t *state, vcu_parameters_t *params,
                   uint32_t dt_ms);

#ifdef __cplusplus
}
#endif

#endif // APPS_H
