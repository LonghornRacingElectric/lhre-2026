#ifndef STATE_MACHINE_LOGIC_H
#define STATE_MACHINE_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#include "state_machine.h"

#define HVC_PRECHARGE_THRESHOLD_PERCENT 0.85f
#define HVC_PRECHARGE_VALID_MS 5000U

typedef struct {
    hvc_state_t state;
    uint32_t prechargeStartTimeMs;
} hvc_state_machine_context_t;

typedef struct {
    bool anyFaults;
    bool shutdownClosed;
    bool chargerConnected;
    float tractiveVoltage;
    float packVoltage;
    uint32_t currentTimeMs;
} hvc_state_machine_inputs_t;

typedef struct {
    bool positiveContactorClosed;
    bool chargerEnabled;
} hvc_state_machine_outputs_t;

void hvc_state_machine_reset(hvc_state_machine_context_t *context,
                             hvc_state_machine_outputs_t *outputs);
void hvc_state_machine_step(hvc_state_machine_context_t *context,
                            const hvc_state_machine_inputs_t *inputs,
                            hvc_state_machine_outputs_t *outputs);

#endif // STATE_MACHINE_LOGIC_H
