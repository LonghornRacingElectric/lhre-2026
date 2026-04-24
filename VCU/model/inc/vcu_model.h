#ifndef VCU_MODEL_H
#define VCU_MODEL_H

#include "vcu_inputs.h"
#include "vcu_outputs.h"
#include "vcu_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "APPS.h"
#include "BSE.h"
#include "Cooling.h"
#include "PRNDL.h"
#include "TorqueMap.h"
#include "TractionControl.h"

typedef struct {
  prndl_machine_t prndl_machine;
  uint32_t time_ms;
  vcu_parameters_t params;
  apps_state_t apps_state;
  bse_state_t bse_state;
  cooling_state_t cooling_state;
  torque_map_state_t torque_map_state;
  traction_control_state_t traction_control_state;
} vcu_model_context_t;

/* Initialize internal model state (call once at startup) */
void vcu_model_init(vcu_model_context_t *ctx, const vcu_parameters_t *params);

/* One control step (call periodically, e.g. every 50 ms) */
void vcu_model_step(vcu_model_context_t *ctx, const vcu_inputs_t *in,
                    vcu_outputs_t *out, uint32_t dt_ms);

#ifdef __cplusplus
}
#endif

#endif /* VCU_MODEL_H */
