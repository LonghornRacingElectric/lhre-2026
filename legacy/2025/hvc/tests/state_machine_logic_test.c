#include <assert.h>

#include "state_machine_logic.h"

static hvc_state_machine_inputs_t inputs(void) {
    return (hvc_state_machine_inputs_t){
        .packVoltage = 500.0f,
    };
}

int main(void) {
    hvc_state_machine_context_t context;
    hvc_state_machine_outputs_t outputs;
    hvc_state_machine_inputs_t in = inputs();

    assert(HVC_STATE_NOT_ENERGIZED == 0);
    assert(HVC_STATE_PRECHARGING == 1);
    assert(HVC_STATE_ENERGIZED == 2);
    assert(HVC_STATE_CHARGING_PRECHARGING == 3);
    assert(HVC_STATE_CHARGING == 4);

    hvc_state_machine_reset(&context, &outputs);
    assert(context.state == HVC_STATE_NOT_ENERGIZED);
    assert(!outputs.positiveContactorClosed);
    assert(!outputs.chargerEnabled);

    in.anyFaults = true;
    in.shutdownClosed = true;
    hvc_state_machine_step(&context, &in, &outputs);
    assert(context.state == HVC_STATE_NOT_ENERGIZED);
    assert(!outputs.positiveContactorClosed);

    in = inputs();
    in.shutdownClosed = true;
    hvc_state_machine_step(&context, &in, &outputs);
    assert(context.state == HVC_STATE_PRECHARGING);

    in.tractiveVoltage = 430.0f;
    in.currentTimeMs = 4999U;
    hvc_state_machine_step(&context, &in, &outputs);
    assert(context.state == HVC_STATE_PRECHARGING);
    assert(!outputs.positiveContactorClosed);

    in.currentTimeMs = 5000U;
    hvc_state_machine_step(&context, &in, &outputs);
    assert(context.state == HVC_STATE_ENERGIZED);
    assert(outputs.positiveContactorClosed);

    in.shutdownClosed = false;
    in.currentTimeMs = 5100U;
    hvc_state_machine_step(&context, &in, &outputs);
    assert(context.state == HVC_STATE_NOT_ENERGIZED);
    assert(!outputs.positiveContactorClosed);

    hvc_state_machine_reset(&context, &outputs);
    in = inputs();
    in.shutdownClosed = true;
    hvc_state_machine_step(&context, &in, &outputs);
    in.currentTimeMs = 100U;
    hvc_state_machine_step(&context, &in, &outputs);
    in.tractiveVoltage = 430.0f;
    in.currentTimeMs = 4000U;
    hvc_state_machine_step(&context, &in, &outputs);
    in.tractiveVoltage = 400.0f;
    in.currentTimeMs = 4500U;
    hvc_state_machine_step(&context, &in, &outputs);
    in.tractiveVoltage = 430.0f;
    in.currentTimeMs = 9499U;
    hvc_state_machine_step(&context, &in, &outputs);
    assert(context.state == HVC_STATE_PRECHARGING);
    in.currentTimeMs = 9500U;
    hvc_state_machine_step(&context, &in, &outputs);
    assert(context.state == HVC_STATE_ENERGIZED);

    in.anyFaults = true;
    hvc_state_machine_step(&context, &in, &outputs);
    assert(context.state == HVC_STATE_NOT_ENERGIZED);
    assert(!outputs.positiveContactorClosed);

    hvc_state_machine_reset(&context, &outputs);
    in = inputs();
    in.shutdownClosed = true;
    in.chargerConnected = true;
    hvc_state_machine_step(&context, &in, &outputs);
    assert(context.state == HVC_STATE_CHARGING_PRECHARGING);
    in.tractiveVoltage = 430.0f;
    in.currentTimeMs = 5000U;
    hvc_state_machine_step(&context, &in, &outputs);
    assert(context.state == HVC_STATE_CHARGING);
    assert(outputs.positiveContactorClosed);
    assert(outputs.chargerEnabled);

    in.chargerConnected = false;
    in.currentTimeMs = 5100U;
    hvc_state_machine_step(&context, &in, &outputs);
    assert(context.state == HVC_STATE_NOT_ENERGIZED);
    assert(!outputs.positiveContactorClosed);
    assert(!outputs.chargerEnabled);

    return 0;
}
