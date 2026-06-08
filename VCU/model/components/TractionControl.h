#ifndef TRACTION_CONTROL_H
#define TRACTION_CONTROL_H

#include "low_pass.h"
#include "vcu_inputs.h"
#include "vcu_outputs.h"
#include "vcu_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  float prev_motor_speed_rpm;
  ema_filter_t accel_filter;
  float mtte_nm;
} traction_control_state_t;

void traction_control_init(traction_control_state_t *state,
                           vcu_parameters_t *params);

void traction_control_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                               traction_control_state_t *state,
                               vcu_parameters_t *params, uint32_t dt_ms);

#ifdef __cplusplus
}
#endif

#endif // TRACTION_CONTROL_H
