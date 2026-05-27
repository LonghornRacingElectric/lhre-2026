#ifndef BATTERY_H
#define BATTERY_H

#include "low_pass.h"
#include "vcu_inputs.h"
#include "vcu_outputs.h"
#include "vcu_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  ema_filter_t cell_voltage_filter;
  float remembered_open_circuit_cell_voltage;
} battery_state_t;

void battery_init(battery_state_t *state, const vcu_parameters_t *params);

void battery_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                      battery_state_t *state, const vcu_parameters_t *params,
                      uint32_t dt_ms);

#ifdef __cplusplus
}
#endif

#endif // BATTERY_H
