#ifndef COOLING_H
#define COOLING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "vcu_inputs.h"
#include "vcu_outputs.h"
#include "vcu_parameters.h"

typedef struct {

} cooling_state_t;

void cooling_init(cooling_state_t *state, const vcu_parameters_t *params);

void cooling_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                      cooling_state_t *state, const vcu_parameters_t *params,
                      uint32_t dt_ms);

#ifdef __cplusplus
}
#endif

#endif // COOLING_H