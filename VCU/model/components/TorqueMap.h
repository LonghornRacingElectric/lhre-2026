#ifndef TORQUE_MAP_H
#define TORQUE_MAP_H

#include "vcu_inputs.h"
#include "vcu_outputs.h"
#include "vcu_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  TORQUE_MAP_LAUNCH_STATE_IDLE = 0,
  TORQUE_MAP_LAUNCH_STATE_PRELOAD = 1,
  TORQUE_MAP_LAUNCH_STATE_RAMP = 2,
  TORQUE_MAP_LAUNCH_STATE_COMPLETE = 3,
} torque_map_launch_state_t;

typedef struct {
  float ocv_estimate_v;
  bool ocv_initialized;

  float filtered_current_a;
  bool current_initialized;

  float filtered_power_w;
  bool power_initialized;

  float integral_w_s;
  float previous_measured_power_w;
  bool has_power_history;

  torque_map_launch_state_t launch_state;
  float launch_preload_elapsed_ms;
  float launch_decel_elapsed_ms;
  float launch_filtered_motor_rpm;
  float launch_previous_filtered_motor_rpm;
  bool launch_rpm_initialized;
  bool launch_has_rpm_history;
  float launch_motor_accel_rpm_per_s;
  bool launch_saw_positive_accel;
  bool launch_contact_detected;
  float launch_output_torque_nm;
} torque_map_state_t;

void torque_map_init(torque_map_state_t *state);

/**
 * @brief Calculates torque request given a set of parameters and inputs
 * @note the torque command is in Nm
 */
void torque_map_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                         torque_map_state_t *state, vcu_parameters_t *params,
                         uint32_t dt_ms);

#ifdef __cplusplus
}
#endif

#endif // TORQUE_MAP_H
