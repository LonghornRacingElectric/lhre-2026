#ifndef VCU_MODEL_H
#define VCU_MODEL_H

#include "vcu_inputs.h"
#include "vcu_outputs.h"
#include "vcu_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  PRNDL_PARK,
  PRNDL_DRIVE,
} prndl_state_t;

/* Initialize internal model state (call once at startup) */
void vcu_model_init(vcu_parameters_t *params);

/* One control step (call periodically, e.g. every 50 ms) */
void vcu_model_step(const vcu_inputs_t *in, vcu_outputs_t *out, uint32_t dt_ms);

#ifdef __cplusplus
}
#endif

#endif /* VCU_MODEL_H */
