#ifndef TRACTION_CONTROL_H
#define TRACTION_CONTROL_H

#include "vcu_inputs.h"
#include "vcu_outputs.h"
#include "vcu_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  TC_STATE_DISABLED = 0, TC_STATE_INHIBITED, TC_STATE_LAUNCH,
  TC_STATE_TRACKING, TC_STATE_LIMITING, TC_STATE_DEGRADED
} tc_state_code_t;
enum {
  TC_FAULT_CONFIG = 1u << 0, TC_FAULT_TIMING = 1u << 1,
  TC_FAULT_WHEEL = 1u << 2, TC_FAULT_MOTION = 1u << 3,
  TC_FAULT_FRONT_DISAGREEMENT = 1u << 4, TC_FAULT_REVERSE = 1u << 5,
  TC_FAULT_DEMAND = 1u << 6, TC_FAULT_SEQUENCE = 1u << 7
};
typedef struct {
  float integral_trim_nm;
  float torque_ceiling_nm;
  float launch_elapsed_s;
  uint32_t last_now_us;
  uint32_t last_timestamp_us[4];
  uint16_t last_sequence[4];
  bool seen_wheel[4];
  bool clock_initialized;
  bool ceiling_initialized;
  bool launch_pending;
  bool launch_active;
  tc_mode_t previous_mode;
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
