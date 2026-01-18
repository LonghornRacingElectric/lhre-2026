#ifndef VCU_MODEL_H
#define VCU_MODEL_H

#include "vcu_inputs.h"
#include "vcu_outputs.h"

/* Initialize internal model state (call once at startup) */
void vcu_model_init(void);

/* One control step (call periodically, e.g. every 50 ms) */
void vcu_model_step(const vcu_inputs_t *in, vcu_outputs_t *out);

#endif /* VCU_MODEL_H */
