#ifndef APPS_H
#define APPS_H

#include "low_pass.h"
#include "vcu_inputs.h"
#include "vcu_outputs.h"
#include "vcu_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  bool apps_implaus;
  uint32_t apps_implaus_ms;
  ema_filter_t pedal_filter;
} apps_state_t;

void apps_init(apps_state_t *state);

float apps_adc_to_travel(float adc, float min, float max);

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
