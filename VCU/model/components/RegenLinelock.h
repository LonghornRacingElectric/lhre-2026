#ifndef REGEN_LINELOCK_H
#define REGEN_LINELOCK_H

#include "vcu_inputs.h"
#include "vcu_outputs.h"
#include "vcu_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  bool ocv_regen_allowed;
  bool current_hard_cut_latched;
  uint32_t linelock_commanded_closed_ms;
} regen_linelock_state_t;

void regen_linelock_init(regen_linelock_state_t *state,
                         const vcu_parameters_t *params);

float regen_linelock_measured_pack_regen_current_a(
    float dc_bus_current_a, const vcu_parameters_t *params);

float regen_linelock_estimated_pack_ocv_v(float terminal_voltage_v,
                                          float dc_bus_current_a,
                                          const vcu_parameters_t *params);

float regen_linelock_available_pack_current_a(float pack_ocv_v,
                                              const vcu_parameters_t *params);

float regen_linelock_torque_limit_nm(float motor_speed_rpm, float pack_ocv_v,
                                     const vcu_parameters_t *params);

void regen_linelock_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                             regen_linelock_state_t *state,
                             const vcu_parameters_t *params, uint32_t dt_ms);

#ifdef __cplusplus
}
#endif

#endif // REGEN_LINELOCK_H
