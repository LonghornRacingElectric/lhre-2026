#ifndef PRNDL_H
#define PRNDL_H

#include "vcu_inputs.h"
#include "vcu_outputs.h"
#include "vcu_parameters.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  PRNDL_PARK = 0,
  PRNDL_DRIVE = 1,
} prndl_state_t;

typedef struct {
  prndl_state_t state;
  bool prev_drive_switch;
  uint32_t drive_start_time_ms;
} prndl_machine_t;

void prndl_init(prndl_machine_t *machine);

/**
 * @brief Evaluates the PRNDL state machine and updates outputs if necessary.
 * Returns the current state of the machine.
 */
void prndl_evaluate(prndl_machine_t *machine, const vcu_inputs_t *in,
                    vcu_outputs_t *out, uint32_t time_ms);

#ifdef __cplusplus
}
#endif

#endif // PRNDL_H
