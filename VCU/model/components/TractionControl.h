#ifndef TRACTION_CONTROL_H
#define TRACTION_CONTROL_H

#include "vcu_inputs.h"
#include "vcu_outputs.h"
#include "vcu_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TC_SENSOR_FRONT_LEFT_INVALID (1u << 0)
#define TC_SENSOR_FRONT_RIGHT_INVALID (1u << 1)
#define TC_SENSOR_REAR_LEFT_INVALID (1u << 2)
#define TC_SENSOR_REAR_RIGHT_INVALID (1u << 3)
#define TC_SENSOR_MOTOR_SPEED_INVALID (1u << 4)
#define TC_SENSOR_FRONT_DISAGREE (1u << 5)
#define TC_SENSOR_REAR_DISAGREE (1u << 6)
#define TC_SENSOR_MOTOR_REAR_DISAGREE (1u << 7)
#define TC_SENSOR_REFERENCE_ACCEL_IMPLAUS (1u << 8)
#define TC_SENSOR_NO_REFERENCE_SPEED (1u << 9)
#define TC_SENSOR_NO_DRIVEN_SPEED (1u << 10)

typedef struct {
  float filtered_vehicle_speed_mps;
  bool vehicle_speed_initialized;

  float filtered_driven_speed_mps;
  bool driven_speed_initialized;

  float filtered_slip_ratio;
  bool slip_initialized;

  float filtered_slip_error;
  bool slip_error_initialized;

  float slip_integral;
  float previous_slip_ratio;
  bool has_slip_history;

  float previous_vehicle_speed_mps;
  float previous_driven_speed_mps;
  bool has_vehicle_speed_history;

  float previous_fl_mps;
  bool has_previous_fl_speed;

  float previous_fr_mps;
  bool has_previous_fr_speed;

  float torque_limit_nm;
  bool torque_limit_initialized;
} traction_control_state_t;

void traction_control_init(traction_control_state_t *state);

/**
 * @brief Reduces out->torque_cmd when driven-wheel slip exceeds the target.
 *
 * This component is deliberately fail-open: disabling TC or losing optional
 * wheel/acceleration inputs never increases torque and never cuts drive torque
 * by itself. It only clamps an already-computed torque request downward.
 */
void traction_control_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                               traction_control_state_t *state,
                               const vcu_parameters_t *params,
                               uint32_t dt_ms);

#ifdef __cplusplus
}
#endif

#endif // TRACTION_CONTROL_H
