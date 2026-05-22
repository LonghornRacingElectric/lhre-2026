#ifndef POWER_LIMIT_H
#define POWER_LIMIT_H

#include "low_pass.h"
#include "vcu_inputs.h"
#include "vcu_outputs.h"
#include "vcu_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  float integral;
  // ema_filter_t power_filter;
} power_limit_state_t;

void power_limit_init(power_limit_state_t *state, vcu_parameters_t *params);

void power_limit_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                          power_limit_state_t *state, vcu_parameters_t *params,
                          uint32_t dt_ms);

#ifdef __cplusplus
}
#endif

#endif // POWER_LIMIT_H
